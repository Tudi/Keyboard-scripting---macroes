#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0500

#include "StdAfx.h"

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "interception.lib")

#define MAX_THREADS				2

GlobalStateStore GlobalStore;

#include <winuser.h>

int main( int argc, char **argv ) 
{   
	//let's not trust random
	GlobalStore.ActiveWindowName = NULL;
	GlobalStore.WorkerThreadAlive = GlobalStore.Sync1Stamp = GlobalStore.Sync2Stamp = 0;
	GlobalStore.MouseX = GlobalStore.MouseY = GlobalStore.RefocusWindow = GlobalStore.MouseXLimitMin = GlobalStore.MouseYLimitMin = GlobalStore.MouseXLimitMax = GlobalStore.MouseYLimitMax = 0;
	GlobalStore.TrackedMouseX = GlobalStore.TrackedMouseY = GlobalStore.DosBoxWidth = GlobalStore.DosBoxHeight = GlobalStore.TrackedMouseScriptX = GlobalStore.TrackedMouseScriptY = 0;
	GlobalStore.TrackedMouseScriptStamp = 0;
	GlobalStore.PrintKeysPressed = false;
	GlobalStore.fMyPMs = NULL;
	GlobalStore.WorkerThreadLoopDuration = 2000;
	GlobalStore.PauseSendKeys = 1;
	GlobalStore.PauseToggleKeyCode = 0;
	GlobalStore.StartedRecording = 0;

	printf("Loading config file...\n");
	LoadSettingsFromFile( );

	if( argc > 1 )
	{
		if( atoi( argv[1] ) == 1 )
		{
			printf("scanning for keyboard and mouse codes until '~' is pressed\n");
			GlobalStore.WorkerThreadAlive = 1;
			GlobalStore.PrintKeysPressed = true;
			KeyScannerThread( NULL );
		}
		else if( atoi( argv[1] ) == 2 )
		{
			//check if the game window is active
			printf("Printing currently active window titles for copy paste purpuse\n");
			GlobalStore.PrintKeysPressed = true;
			EnumWindows( EnumWindowsProcSetActive, NULL );
		}
		else if( atoi( argv[1] ) == 3 )
		{
			//check if the game window is active
			printf("Printing mouse cords until force close:\n");
			while( 1 )
			{
				POINT p;
				if( GetCursorPos( &p ) )
				{
					int Width, Height;
					GetDesktopResolution( Width, Height );
					int NormalizedMouseX = 65535 * p.x / Width;
					int NormalizedMouseY = 65535 * p.y / Height;
					printf("Mouse position now : %d %d : Normalized %d %d : Tracked %d %d\n", p.x, p.y, NormalizedMouseX, NormalizedMouseY, GlobalStore.TrackedMouseX, GlobalStore.TrackedMouseY );
				}
				Sleep( 500 );
			}
		}
		return 0;
	}

    WSADATA wsaData;
    int iResult;
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != NO_ERROR)
	{
      printf("WSAStartup failed: %d\n", iResult);
      return 1;
    }

	//init threads to run the logic
	printf("Starting worker threads...\n");
	DWORD   dwThreadIdArray[MAX_THREADS];
	HANDLE  hThreadArray[MAX_THREADS]; 
	GlobalStore.WorkerThreadAlive = 1;
	{
		printf("\t Starting keyboard sender voter thread...\n");
		hThreadArray[0] = CreateThread( 
			NULL,                   // default security attributes
			0,                      // use default stack size  
			KeyWriterThread,		// thread function name
			0,						// argument to thread function 
			0,                      // use default creation flags 
			&dwThreadIdArray[0]);   // returns the thread identifier 
		if (hThreadArray[0] == NULL) 
		   ExitProcess(3);

		printf("\t Starting keyboard driver thread...\n");
		hThreadArray[1] = CreateThread( 
			NULL,                   // default security attributes
			0,                      // use default stack size  
			KeyScannerThread,		// thread function name
			0,						// argument to thread function 
			0,                      // use default creation flags 
			&dwThreadIdArray[1]);   // returns the thread identifier 
		if (hThreadArray[1] == NULL) 
		   ExitProcess(3);
	}

	//don't really need this, we could use the waitmultiplethreads, but then irc client might block
	printf("\tStarting console listener thread...\n");
	LoopListenConsole();
//	MoveMouseAsDemo();

	printf("Main Thread : Started shutdown \n");

	WSACleanup();

	{
		printf("Waiting for worker threads to exit\n");
		GlobalStore.WorkerThreadAlive = 0;
		WaitForMultipleObjects(MAX_THREADS, hThreadArray, TRUE, INFINITE);

		// Close all thread handles and free memory allocations.
		for(int i=0; i<MAX_THREADS; i++)
			CloseHandle(hThreadArray[i]);
	}

    return 0;   
}