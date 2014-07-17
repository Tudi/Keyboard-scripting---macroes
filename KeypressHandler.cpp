#include "StdAfx.h"
#include <windows.h>

//does not work
//http://msdn.microsoft.com/en-us/library/windows/desktop/dd375731%28v=vs.85%29.aspx
void SendKeyPress1( int code )
{
    INPUT ip;
    ip.type = INPUT_KEYBOARD;
    ip.ki.wScan = 0;
    ip.ki.time = 0;
    ip.ki.dwExtraInfo = 0;
//			ip.ki.wVk = 0x45; // E
	ip.ki.wVk = 0x12; // Alt
	ip.ki.dwFlags = 0; // 0 for key press
	SendInput(1, &ip, sizeof(INPUT));
	Sleep( SLEEP_BETWEEN_KEYPRESS );
	ip.ki.dwFlags = KEYEVENTF_KEYUP; // KEYEVENTF_KEYUP for key release
	SendInput(1, &ip, sizeof(INPUT));
}

//does not work
void SendKeyPress2( int code )
{
    INPUT ip;
    ip.type = INPUT_KEYBOARD;
    ip.ki.wScan = 0;
    ip.ki.time = 0;
    ip.ki.dwExtraInfo = 0;
	ip.ki.wVk = 0; 
	ip.ki.wScan = MapVirtualKey( 0x12, MAPVK_VK_TO_VSC);
	ip.ki.dwFlags = KEYEVENTF_SCANCODE; // 0 for key press
	SendInput(1, &ip, sizeof(INPUT));
	Sleep( SLEEP_BETWEEN_KEYPRESS );
	ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP; // KEYEVENTF_KEYUP for key release
	SendInput(1, &ip, sizeof(INPUT));
}

void SendKeyPress3( int code, int PressDownDelay, int ReleaseDelay )
{
	InterceptionKeyStroke kstroke;
	memset( &kstroke, 0, sizeof( kstroke ) );

	//you can push down a code and leave it like that
	if( PressDownDelay >= 0 )
	{
		kstroke.code = code;
		kstroke.state = 0;
		interception_send( GlobalStore.context, GlobalStore.KeyboardDevice, (InterceptionStroke *)&kstroke, 1);
		if( PressDownDelay == 0 && ReleaseDelay >= 0 )
			Sleep( SLEEP_BETWEEN_KEYPRESS );
		else if( PressDownDelay > 0 )
			Sleep( PressDownDelay );
	}

	//you can push down a code and leave it like that
	if( ReleaseDelay >= 0 )
	{
		kstroke.code = code;
		kstroke.state = 1;	
		interception_send( GlobalStore.context, GlobalStore.KeyboardDevice, (InterceptionStroke *)&kstroke, 1);
		if( ReleaseDelay > 0 )
			Sleep( ReleaseDelay );
	}
}

void SendMouseChange( int state, int flags, int Xchange, int Ychange, int PressDownDelay, int ReleaseDelay )
{
	InterceptionMouseStroke mstroke;
	memset( &mstroke, 0, sizeof( mstroke ) );

//printf( "sending mouse state change %d x %d y %d down %d up %d \n", state, Xchange, Ychange, PressDownDelay, ReleaseDelay );
	mstroke.state = state;
	mstroke.x = Xchange;
	mstroke.y = Ychange;
	if( flags == 0 )
		mstroke.flags = INTERCEPTION_MOUSE_MOVE_RELATIVE;
	else
		mstroke.flags = flags;

	if( PressDownDelay >= 0 )
	{
		interception_send( GlobalStore.context, GlobalStore.MouseDevice, (InterceptionStroke *)&mstroke, 1);
		if( PressDownDelay == 0 && ReleaseDelay > 0 )
			Sleep( SLEEP_BETWEEN_KEYPRESS );
		else
			Sleep( PressDownDelay );
	}

	if( ReleaseDelay >= 0 )
	{
		if( state == INTERCEPTION_MOUSE_LEFT_BUTTON_DOWN || state == INTERCEPTION_MOUSE_RIGHT_BUTTON_DOWN )
		{
			mstroke.state = 2 * state;
			mstroke.x = 0;
			mstroke.y = 0;
			interception_send( GlobalStore.context, GlobalStore.MouseDevice, (InterceptionStroke *)&mstroke, 1);
			if( ReleaseDelay != 0 )
				Sleep( ReleaseDelay );
		}
	}
}

DWORD WINAPI KeyScannerThread( LPVOID lpParam )
{
	GlobalStore.context = interception_create_context();

	InterceptionDevice		LastDevice;
	InterceptionStroke		LastStroke;
	int		PrevKeyEventTick = GetTickCount();
	int		PrevMouseEventTick = GetTickCount();

	bool	HasKeyboardHandlers = false;
	bool	HasMouseHandlers = false;

	std::list<IrcGameKeyStore*>::iterator itr;
	for( itr=GlobalStore.MonitoredKeys.begin(); itr!=GlobalStore.MonitoredKeys.end(); itr++ )
	{
		if( (*itr)->StrokeCode != 0 )
			HasKeyboardHandlers = true;
		if( (*itr)->MouseX != 0 || (*itr)->MouseY != 0 || (*itr)->MouseKey != 0 )
			HasMouseHandlers = true;
	}

	if( GlobalStore.PrintKeysPressed == true )
	{
		HasKeyboardHandlers = true;
		HasMouseHandlers = true;
	}

	if( HasKeyboardHandlers )
		interception_set_filter( GlobalStore.context, interception_is_keyboard, INTERCEPTION_FILTER_KEY_DOWN | INTERCEPTION_FILTER_KEY_UP);
	if( HasMouseHandlers )
		interception_set_filter( GlobalStore.context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_MOVE | INTERCEPTION_MOUSE_LEFT_BUTTON_DOWN | INTERCEPTION_MOUSE_LEFT_BUTTON_UP | INTERCEPTION_MOUSE_RIGHT_BUTTON_DOWN | INTERCEPTION_MOUSE_RIGHT_BUTTON_UP );
//		interception_set_filter( GlobalStore.context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_ALL );
	while( interception_receive( GlobalStore.context, LastDevice = interception_wait( GlobalStore.context ), (InterceptionStroke *)&LastStroke, 1) > 0
		&& GlobalStore.WorkerThreadAlive == 1
//		&& 0
		)
	{
		if( interception_is_mouse( LastDevice ) )
		{
			GlobalStore.MouseDevice = LastDevice;
            InterceptionMouseStroke &mstroke = *(InterceptionMouseStroke *) &LastStroke;
			
			memcpy( &GlobalStore.MouseStroke, &LastStroke, sizeof( InterceptionMouseStroke ) );

			//try to track mouse coordinate
            if( (mstroke.flags & INTERCEPTION_MOUSE_MOVE_ABSOLUTE) == 0 )
			{
				//this can go off screen
				GlobalStore.TrackedMouseX += mstroke.x;
				if( GlobalStore.TrackedMouseX > GlobalStore.DosBoxWidth )
					GlobalStore.TrackedMouseX = GlobalStore.DosBoxWidth;
				if( GlobalStore.TrackedMouseX < 0 )
					GlobalStore.TrackedMouseX = 0;
				GlobalStore.TrackedMouseY += mstroke.y;
				if( GlobalStore.TrackedMouseY > GlobalStore.DosBoxHeight )
					GlobalStore.TrackedMouseY = GlobalStore.DosBoxHeight;
				if( GlobalStore.TrackedMouseY < 0 )
					GlobalStore.TrackedMouseY = 0;
			}

			if( GlobalStore.StartedRecording == 1 )
			{
				//track mouse positions and on click try to print it out
				GlobalStore.TrackedMouseScriptX += mstroke.x;
				if( GlobalStore.TrackedMouseScriptX > GlobalStore.MouseXLimitMax )
					GlobalStore.TrackedMouseScriptX = GlobalStore.MouseXLimitMax;
				if( GlobalStore.TrackedMouseScriptX < GlobalStore.MouseXLimitMin )
					GlobalStore.TrackedMouseScriptX = GlobalStore.MouseXLimitMin;
				GlobalStore.TrackedMouseScriptY += mstroke.y;
				if( GlobalStore.TrackedMouseScriptY > GlobalStore.MouseYLimitMax )
					GlobalStore.TrackedMouseScriptY = GlobalStore.MouseYLimitMax;
				if( GlobalStore.TrackedMouseScriptY < GlobalStore.MouseYLimitMin )
					GlobalStore.TrackedMouseScriptY = GlobalStore.MouseYLimitMin;

				if( mstroke.state != 0 )
				{
					int TimePassed = GetTickCount() - GlobalStore.TrackedMouseScriptStamp;
					GlobalStore.TrackedMouseScriptStamp = GetTickCount();
					POINT p;
					GetCursorPos( &p );
//					printf("Registering click at (%d,%d), Time spent %d. Win said %d,%d\n", GlobalStore.TrackedMouseScriptX, GlobalStore.TrackedMouseScriptY, TimePassed, p.x, p.y );
					printf("click (%d,%d) - %d.\n", p.x, p.y, TimePassed );
//					GlobalStore.TrackedMouseScriptX = p.x;
//					GlobalStore.TrackedMouseScriptY = p.y;
				}
			}

            interception_send( GlobalStore.context, GlobalStore.MouseDevice, (InterceptionStroke *)&mstroke, 1);

			if( GlobalStore.PrintKeysPressed )
			{
//				printf( "Mouse %d: state %d, x %d y %d, flags %d, rolling %d, info %d, tracked %d %d\n", GetTickCount() - PrevKeyEventTick, mstroke.state, mstroke.x, mstroke.y, mstroke.flags, mstroke.rolling, mstroke.information, GlobalStore.TrackedMouseX, GlobalStore.TrackedMouseY );
				printf( "Mouse %d: state %d, x %d y %d, flags %d, rolling %d, info %d\n", GetTickCount() - PrevKeyEventTick, mstroke.state, mstroke.x, mstroke.y, mstroke.flags, mstroke.rolling, mstroke.information );
				PrevKeyEventTick = GetTickCount();
			}
		}
		if( interception_is_keyboard( LastDevice ) )
		{
			GlobalStore.KeyboardDevice = LastDevice;
            InterceptionKeyStroke &kstroke = *(InterceptionKeyStroke *) &LastStroke;

			memcpy( &GlobalStore.KeyboardStroke, &LastStroke, sizeof( InterceptionKeyStroke ) );

			interception_send( GlobalStore.context, GlobalStore.KeyboardDevice, (InterceptionStroke *)&LastStroke, 1);

			if( GlobalStore.PrintKeysPressed )
			{
				printf( "Keyboard delay %d: scan code %d %d %d\n", GetTickCount() - PrevKeyEventTick, kstroke.code, kstroke.state, kstroke.information );
				PrevKeyEventTick = GetTickCount();
			}
			else if( kstroke.state == 0 ) //pushdown
			{
				HWND FW = GetForegroundWindow( );
				HWND CW = GetConsoleWindow();
				if( CW == FW )
					printf( "(%d)", kstroke.code );
			}

			if( kstroke.code == GlobalStore.PauseToggleKeyCode && kstroke.state == 0 )
			{
				GlobalStore.PauseSendKeys = 1 - GlobalStore.PauseSendKeys;
				printf( "KeySending thread pause state changed to %d\n", GlobalStore.PauseSendKeys );
				std::list<IrcGameKeyStore*>::iterator itr;
				for( itr=GlobalStore.MonitoredKeys.begin(); itr!=GlobalStore.MonitoredKeys.end(); itr++ )
					if( (*itr)->PushInterval == ONE_TIME_PUSH_KEY_INTERVAL )
					{
						(*itr)->PushInterval = 0;
						(*itr)->LastPushStamp = GetTickCount();
					}

			}

			if( kstroke.code == 10 && kstroke.state == 0 )
			{
				GlobalStore.StartedRecording = 1 - GlobalStore.StartedRecording;
				if( GlobalStore.StartedRecording == 1 )
				{
					//bring mouse to 0 0
//					SendMouseChange( 0, 0, -10000, -10000, SLEEP_BETWEEN_KEYPRESS, SLEEP_BETWEEN_KEYPRESS );
//					SendMouseChange( 1, 0, -10000, -10000, SLEEP_BETWEEN_KEYPRESS, SLEEP_BETWEEN_KEYPRESS );
//					SendMouseChange( 1, 0, 0, 0, SLEEP_BETWEEN_KEYPRESS, SLEEP_BETWEEN_KEYPRESS );
					GlobalStore.TrackedMouseScriptX = 0;
					GlobalStore.TrackedMouseScriptY = 0;
					printf("Key 9 was pressed. Moved mouse to 0,0 so we can start recording mouse movement for scripts. Not accurate !\n");
				}
			}

			if( GlobalStore.StartedRecording == 1 )
			{
				if( kstroke.state != 0 )
				{
					int TimePassed = GetTickCount() - GlobalStore.TrackedMouseScriptStamp;
					GlobalStore.TrackedMouseScriptStamp = GetTickCount();
					POINT p;
					GetCursorPos( &p );
					printf("click (%d,%d) - %d.\n", p.x, p.y, TimePassed );
				}
			}

			if( kstroke.code == SCANCODE_CONSOLE )
			{
				GlobalStore.WorkerThreadAlive = 0;
				printf( "Esc pressed. Shutting down\n" );
				break;
			}
		}
	}/**/

	//this happens in case laptop mouse goes idle. It might come back later = Never give up hope !
	if( GlobalStore.WorkerThreadAlive == 1 )
	{
		printf("Worker thread keymonitor device failure. Trying to work with remaining devices...\n");
		while( GlobalStore.WorkerThreadAlive == 1 )
			Sleep( 1000 );
	}
	interception_destroy_context( GlobalStore.context );
	printf("Worker thread keymonitor exited\n");

	return 0;
}

DWORD WINAPI KeyWriterThread( LPVOID lpParam )
{

	while( GlobalStore.WorkerThreadAlive == 1 )
	{
		unsigned int StartStamp = GetTickCount();
		unsigned int GlobalCooldown = GlobalStore.WorkerThreadLoopDuration;

		int CanProcessKeys = 1;
		if( GlobalStore.PauseSendKeys == 0 )
		{
			//check if we are restricted taking actions only in 1 window
			if( GlobalStore.ActiveWindowName )
			{
				HWND ActiveWindow = IsWindowOurFocusWindow( 0 , 0 );
				if( ActiveWindow == 0 )
				{
					HWND ActiveWindow = GetForegroundWindow();
					char ActiveWindowTitle[DEFAULT_BUFLEN];
					GetWindowText( ActiveWindow, ActiveWindowTitle, DEFAULT_BUFLEN );
					if( GlobalStore.RefocusWindow != 0 )
					{
						printf("Game window is not active. Trying to focus on it. We need / have : \n\t%s \n\t%s\n", GlobalStore.ActiveWindowName, ActiveWindowTitle );
						EnumWindows( EnumWindowsProcSetActive, NULL );
					}
					else
					{
						printf("Game window is not active. Skipping actions. We need / have : \n\t%s \n\t%s\n", GlobalStore.ActiveWindowName, ActiveWindowTitle );
					}
					CanProcessKeys = 2;
				}
			}
		}
		else
			CanProcessKeys = 3;

		if( CanProcessKeys == 1 )
		{
			int PrevPlayerGroupUpdated = -1;
			int PlayerGroupNotYetUpdated = -1;
			std::list<IrcGameKeyStore*>::iterator itr;

			do{
				//check if we still have a player group that can be updated with keypresses
				PrevPlayerGroupUpdated = PlayerGroupNotYetUpdated;
				for( itr=GlobalStore.MonitoredKeys.begin(); itr!=GlobalStore.MonitoredKeys.end(); itr++ )
					if( (*itr)->PlayerGroup > PrevPlayerGroupUpdated )
					{
						PlayerGroupNotYetUpdated = (*itr)->PlayerGroup;
						break;
					}

				//we found a player group
				if( PlayerGroupNotYetUpdated != PrevPlayerGroupUpdated )
				{
					//get most voted action
					int BestActionTimeDiff = 0;
					IrcGameKeyStore *BestAction = NULL;
					for( itr=GlobalStore.MonitoredKeys.begin(); itr!=GlobalStore.MonitoredKeys.end(); itr++ )
						if( (*itr)->PlayerGroup == PlayerGroupNotYetUpdated ) 
						{
							int TimeSinceLastPushed = StartStamp - (*itr)->LastPushStamp;
							if( BestActionTimeDiff < TimeSinceLastPushed 
								&& TimeSinceLastPushed >= (*itr)->PushInterval
								)
							{
//								printf("Button %s is getting selected because it was %d since pushed and interval is %d \n", (*itr)->IrcText, TimeSinceLastPushed, (*itr)->PushInterval );
								BestActionTimeDiff = TimeSinceLastPushed;
								BestAction = (*itr);
							}
						}

					//perform the action
					if( BestAction != NULL )
					{
						//mark it as pushed
						BestAction->LastPushStamp = StartStamp;

						//push only once buttons
						if( BestAction->PushInterval == 0 )
							BestAction->PushInterval = ONE_TIME_PUSH_KEY_INTERVAL;

						//for serial scripting
						GlobalCooldown = max( BestAction->GlobalCooldown, GlobalCooldown );

						//actually push it
						if( BestAction->StrokeCode > 0 )
						{
							SendKeyPress3( BestAction->StrokeCode, BestAction->StrokePushdownDelay, BestAction->StrokeReleaseDelay );
							printf("Taking keyboard action : %s\n", BestAction->IrcText );
						}
						if( BestAction->MouseKey != 0 || BestAction->MouseX != 0 || BestAction->MouseY != 0 )
						{
							if( BestAction->MouseMoveType == MOUSE_MOVE_TYPE_ABSOLUTE_NORMALIZED )
							{
								//remember position
								int NormalizedMouseX = -1;
								int NormalizedMouseY = -1;
								GetMouseNormalizedCordsForPixel( NormalizedMouseX, NormalizedMouseY );
								//move to absolute normalized cord location
								SendMouseChange( 0, INTERCEPTION_MOUSE_MOVE_ABSOLUTE, BestAction->MouseX, BestAction->MouseY, SLEEP_BETWEEN_KEYPRESS, SLEEP_BETWEEN_KEYPRESS );
								//do a left mouse click
								SendMouseChange( INTERCEPTION_MOUSE_LEFT_BUTTON_DOWN, 0, 0, 0, SLEEP_BETWEEN_KEYPRESS, SLEEP_BETWEEN_KEYPRESS );
								//move back to old location
								SendMouseChange( 0, INTERCEPTION_MOUSE_MOVE_ABSOLUTE, NormalizedMouseX, NormalizedMouseY, BestAction->StrokePushdownDelay, BestAction->StrokeReleaseDelay );
							}
							//
							else if( BestAction->MouseFlags == MOUSE_MOVE_TYPE_ABSOLUTE_PIXEL )
							{
								//remember position
								int NormalizedMouseX = -1;
								int NormalizedMouseY = -1;
								GetMouseNormalizedCordsForPixel( NormalizedMouseX, NormalizedMouseY );
								//bring mouse to 0 0
								SendMouseChange( 0, 0, -10000, -10000, SLEEP_BETWEEN_KEYPRESS, SLEEP_BETWEEN_KEYPRESS );
								//now bring it to specific pixel coords
								SendMouseChange( 0, 0, BestAction->MouseX, BestAction->MouseY, SLEEP_BETWEEN_KEYPRESS, SLEEP_BETWEEN_KEYPRESS );
								//do a left mouse click
								SendMouseChange( INTERCEPTION_MOUSE_LEFT_BUTTON_DOWN, 0, 0, 0, SLEEP_BETWEEN_KEYPRESS, SLEEP_BETWEEN_KEYPRESS );
								//move back to old location
								SendMouseChange( 0, INTERCEPTION_MOUSE_MOVE_ABSOLUTE, NormalizedMouseX, NormalizedMouseY, BestAction->StrokePushdownDelay, BestAction->StrokeReleaseDelay );
							}
							//dosbox only handles relative mouse coordinate system !
							else if( BestAction->MouseFlags == MOUSE_MOVE_TYPE_ABSOLUTE_PIXEL_DOSBOX )
							{
								//bring mouse to 0 0
								SendMouseChange( 0, 0, -10000, -10000, SLEEP_BETWEEN_KEYPRESS, SLEEP_BETWEEN_KEYPRESS );
								//now bring it to specific coords
								SendMouseChange( 0, 0, BestAction->MouseX, BestAction->MouseY, SLEEP_BETWEEN_KEYPRESS, SLEEP_BETWEEN_KEYPRESS );
								//do a left mouse click
								SendMouseChange( INTERCEPTION_MOUSE_LEFT_BUTTON_DOWN, 0, 0, 0, SLEEP_BETWEEN_KEYPRESS, SLEEP_BETWEEN_KEYPRESS );
							}
							else 
//								if( CanClickWithMouse( BestAction->MouseKey ) )
								SendMouseChange( BestAction->MouseKey, BestAction->MouseFlags, BestAction->MouseX, BestAction->MouseY, BestAction->StrokePushdownDelay, BestAction->StrokeReleaseDelay );

							printf("Taking mouse action : %s\n", BestAction->IrcText );
						}
					}
				}

			}while( PlayerGroupNotYetUpdated != PrevPlayerGroupUpdated 
				&& GlobalStore.WorkerThreadAlive == 1
				);
		}

		unsigned int EndStamp = GetTickCount();
		if( EndStamp - StartStamp < (unsigned int )GlobalCooldown )
		{
			int TimeSpent = EndStamp - StartStamp;
			int NeedToSleep = GlobalCooldown - TimeSpent;
//			printf( "Need to sleep for %d - %d\n", NeedToSleep, TimeSpent );
			Sleep( NeedToSleep );
		}
//		printf( "End Loop\n" );
	}/**/

	return 0;
}

int CanClickWithMouse( int ClickKey )
{
//	printf("Testick canclick %d\n", ClickKey );
	//this is not a click. We can move mouse anywhere
	if( ClickKey == 0 )
		return 1;
	//are we inside a box that denies clicks ?
	POINT p;
	if( GetCursorPos( &p ) )
	{
		std::list<NoMouseClickBox*>::iterator itr;
		for( itr=GlobalStore.DenyMouseActionBoxes.begin(); itr!=GlobalStore.DenyMouseActionBoxes.end(); itr++ )
		{
//			printf("Testing Mouse click %d %d %d -> %d %d %d %d %d\n", ClickKey, p.x, p.y, (*itr)->MouseKey, (*itr)->XStart, (*itr)->YStart, (*itr)->XEnd, (*itr)->YEnd );
			if( (*itr)->XStart <= p.x && p.x <= (*itr)->XEnd
				&& (*itr)->YStart <= p.y && p.y <= (*itr)->YEnd
				&& ( (*itr)->MouseKey & ClickKey ) )
			{
//				printf("Deny clicking with mouse\n");
				return 0;
			}
		}
	}
//	printf("Allow clicking with mouse\n");
	return 1;
}