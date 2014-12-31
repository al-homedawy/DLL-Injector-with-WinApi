#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <fstream>
#include <sstream>

#include "Shlwapi.h"
#include "resource.h"

#pragma comment ( lib, "Shlwapi.lib" )

using namespace std;

template <class T> void dbg ( T msg )
{
	stringstream ss;
	ss << msg;
	string str;
	str += ss.str ();
	MessageBoxA ( 0, str.c_str (), 0, 0 );
}

class UTILITY
{
private:
	HWND hWnd;

	DWORD LocateProcessId ( string processName )
	{
		HANDLE hSnapshot = CreateToolhelp32Snapshot ( TH32CS_SNAPALL, 0 );

		PROCESSENTRY32 pe;
		pe.dwSize = sizeof ( PROCESSENTRY32 );

		if ( Process32First ( hSnapshot, &pe ) )
			while ( Process32Next ( hSnapshot, &pe ) )
				if ( !strcmp ( pe.szExeFile, processName.c_str () ) )
					return pe.th32ProcessID;

		return -1;
	}
	BOOL AllocateAndWrite ( HANDLE hProcess, string DllPath, void** mem )
	{
		SIZE_T szMemWritten;
		*mem = VirtualAllocEx ( hProcess, 0, DllPath.length (), MEM_COMMIT, PAGE_EXECUTE_READWRITE );
		
		if ( !(*mem) )
			return FALSE;
		else
			return WriteProcessMemory ( hProcess, *mem, DllPath.c_str (), DllPath.length (), &szMemWritten );
	}
public:
	UTILITY ( HWND Wnd )
	{
		hWnd = Wnd;
	}

	VOID ListAllProcesses ()
	{
		SendMessageA ( GetDlgItem ( hWnd, IDC_LIST1 ), LB_RESETCONTENT, 0, 0 );

		HANDLE hSnapshot = CreateToolhelp32Snapshot ( TH32CS_SNAPALL, 0 );

		PROCESSENTRY32 pe;
		pe.dwSize = sizeof ( PROCESSENTRY32 );

		if ( Process32First ( hSnapshot, &pe ) )
		{
			SendMessageA ( GetDlgItem ( hWnd, IDC_LIST1 ), LB_ADDSTRING, 0, (LPARAM) pe.szExeFile );

			while ( Process32Next ( hSnapshot, &pe ) )
				SendMessageA ( GetDlgItem ( hWnd, IDC_LIST1 ), LB_ADDSTRING, 0, (LPARAM) pe.szExeFile );
		}
	}
	BOOL InjectLibrary ( string DllPath, string processName )
	{
		DWORD dwProcessID = LocateProcessId ( processName );

		if ( dwProcessID == -1 )
			return FALSE;
		else
		{
			HANDLE hProcess = OpenProcess ( PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, dwProcessID );

			if ( !hProcess )
				return FALSE;
			else
			{
				void* location;
				
				if ( AllocateAndWrite ( hProcess, DllPath, &location ) )
				{
					FARPROC LoadLib = GetProcAddress ( GetModuleHandle ( "kernel32.dll" ), "LoadLibraryA" );

					if ( !location )
					{
						CloseHandle ( hProcess );
						return FALSE;
					}
					else if ( CreateRemoteThread ( hProcess, NULL, NULL, (LPTHREAD_START_ROUTINE) LoadLib, location, NULL, NULL ) )
					{
						CloseHandle ( hProcess );
						return TRUE;
					}
					else
					{
						CloseHandle ( hProcess );
						return FALSE;
					}
				}
				else
				{
					CloseHandle ( hProcess );
					return FALSE;
				}
			}
		}

		return FALSE;
	}
};

BOOL LoadSettings ( HWND hWnd )
{
	if ( PathFileExists ( "\\Settings.ini" ) )
	{
		ifstream File ( "\\Settings.ini" );
		string path;
		File >> path;
		File.close ();
		SetWindowText ( GetDlgItem ( hWnd, IDC_EDIT1 ), path.c_str () );
		return TRUE;
	}
	else
		return FALSE;
}

void SaveSettings ( HWND hWnd )
{
	char chDllPath [256];
	GetWindowText ( GetDlgItem ( hWnd, IDC_EDIT1 ), chDllPath, sizeof ( chDllPath ) );
	
	ofstream File ( "\\Settings.ini" );
	File.clear ();
	File << chDllPath << endl;
	File.close ();
}

INT_PTR CALLBACK MainDlg ( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	UTILITY tools ( hWnd );

	switch ( uMsg )
	{
	case WM_INITDIALOG:
		{
			if ( !LoadSettings ( hWnd ) )
				SetWindowText ( GetDlgItem ( hWnd, IDC_EDIT1 ), "C:\\" );
			tools.ListAllProcesses ();
			return TRUE;
		}

	case WM_COMMAND:
		{
			switch ( wParam )
			{
			case IDC_BUTTON1:
				{
					char szBuffer [256];
					OPENFILENAME ofn;
					RtlZeroMemory ( &ofn, sizeof ( OPENFILENAME ) );
					ofn.lStructSize = sizeof ( OPENFILENAME );
					ofn.hwndOwner = hWnd;
					ofn.lpstrFile = szBuffer;
					ofn.lpstrFile [0] = '\0';
					ofn.lpstrFilter = "DLL Files\0*.DLL*\0\0";
					ofn.nFilterIndex = 1;
					ofn.nMaxFile = sizeof ( szBuffer );
					ofn.lpstrInitialDir = "C:\\";
					ofn.lpstrFileTitle = "Select the desired library (.DLL)";
					ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

					if ( GetOpenFileName ( &ofn ) )
						SetWindowText ( GetDlgItem ( hWnd, IDC_EDIT1 ), szBuffer );
					break;
				}
		
			case IDC_BUTTON2:
				{
					tools.ListAllProcesses ();
					break;
				}

			case IDC_BUTTON3:
				{
					SaveSettings ( hWnd );

					char chEdit [256];
					GetWindowText ( GetDlgItem ( hWnd, IDC_EDIT1 ), chEdit, 256 );
					
					if ( strcmp ( chEdit, "" ) != 0 )
					{
						char chProcessName [256];
						int currentSel = SendMessageA ( GetDlgItem ( hWnd, IDC_LIST1 ), LB_GETCURSEL, 0, 0 );
						
						if ( SendMessageA ( GetDlgItem ( hWnd, IDC_LIST1 ), LB_GETTEXT, (WPARAM) currentSel, (LPARAM) chProcessName ) != LB_ERR )
						{
							string DllPath, processName;
							DllPath += chEdit;
							DllPath += '\0';
							processName += chProcessName;							
							
							if ( tools.InjectLibrary ( DllPath, processName ) )
								MessageBoxA ( hWnd, "Injection Successful", "Injection", MB_ICONINFORMATION );
							else
								MessageBoxA ( hWnd, "Something went wrong!", "Injection", MB_ICONERROR );
						}
						else
							MessageBoxA ( hWnd, "Please select a target process!", "Injection", MB_ICONERROR );
					}
					else
						MessageBoxA ( hWnd, "Check your DLL Path!", "Injection", MB_ICONERROR );

					break;
				}

			case IDC_BUTTON4:
				{
					MessageBoxA ( hWnd, "This program was created by\n   Hussain Al-Homedawy", "Message", MB_ICONINFORMATION );
					break;
				}
			}

			
			return TRUE;
		}

	case WM_CLOSE:
		{
			EndDialog ( hWnd, 0 );
			return TRUE;
		}
	default:
		return FALSE;
	}
}

int CALLBACK WinMain ( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd )
{
	return DialogBox ( hInstance, MAKEINTRESOURCE ( IDD_DIALOG1 ), NULL, MainDlg ) ; 
}