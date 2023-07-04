#include <Windows.h>
#include <windowsx.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include "TextRenderer.h"
#include "Config.h"

//-------------------------Macros-------------------------
#define BALLRADIUS (BALLDIAMETER/2.0f)

//These are the IDs that represent the edges of the window for collision detection
//The idea is that the system will run out of memory way before a ball that can take on one of these ids is created
#define WINDOW_LEFT_ID       0xBfffffffu  
#define WINDOW_TOP_ID         0xCfffffffu
#define WINDOW_RIGHT_ID     0xDfffffffu
#define WINDOW_BOTTOM_ID 0xEfffffffu
//--------------------------Types---------------------------
struct BallObject {
	float xposition; //position in pixels
	float yposition;
	float xvelocity; //velocities in pixels/seconds
	float yvelocity;
	float ximpulse;
	float yimpulse;
};

struct Offset { float x; float y; };

struct CollisionPoint{
	unsigned object1, object2;
	float xposition, yposition;
	float xnormal, ynormal;
};
//-------------------------Constants---------------------------
const float Rate = 240; //rate in hertz (or ticks/seconds)
const float Gravity = 1080; //in pixels / seconds^2
const float GrabForce = 40.0f;
const float Damping = 1.0f;
const float GrabRadiusMultiplier = 1.0f; //Increases the click radius of the ball objects
const float Elasticity = 0.9f;
const float BallMass = 1.0f;
const unsigned char Iterations = 4u; //Used in Collision resoultion, more = accurate, less = fast
const unsigned char FixIterations = 2u; //Remove Overlaps after resolution
const float FixIterationsFactor = 0.6f; //Remove Overlaps after resolution
//------------------------Persistent State--------------------
static float WindowWidth = INITIALWIDTH;
static float WindowHeight = INITIALHEIGHT;
static float XWindowPos = 0.0f;
static float YWindowPos = 0.0f;
static float XWindowVelocity;
static float YWindowVelocity;
static std::vector<BallObject> BallObjects;
static std::vector<CollisionPoint> CollisionPoints;
static CRITICAL_SECTION cs;
static unsigned GrabbedObject = 0xffffffffu;
static float Mousex;
static float Mousey;
static bool Pause = false;
static HANDLE EventID;
static std::unordered_map<std::string, std::string> DebugInfo;
extern HANDLE RenderEvent;
extern HANDLE PhysicsEvent;

std::string GetDebugInfo(const std::string& arg) {
	EnterCriticalSection(&cs);
	std::string ret;
	try {
		ret = DebugInfo.at(arg);
	}
	catch (std::exception e) {
		ret = std::string();
	}
	LeaveCriticalSection(&cs);
	return ret;
}

static void SetDebugInfo(const std::string arg,const std::string& value) {
	EnterCriticalSection(&cs);
	DebugInfo[arg] = value;
	LeaveCriticalSection(&cs);
}

void InitPhysics(void) {
	InitializeCriticalSection(&cs);
	//MyFont = TextSystem.TSCreateFont("C:\\Windows\\Fonts\\arial.ttf", 24);
	//TS_Text_Desc td;
	//td.Xscale = -1.0f;
	//td.Xoffset = 0.0f;
	//td.Yscale = 1.0f;
	//td.Yoffset = -0.15;
	//td.Red = 1.0f;
	//td.Blue = 0.0f;
	//td.Green = 0.0f;
	//td.Alpha = 1.0f;
	//td.ChangeFlags = TS_CONTENT;
	//MyText = TextSystem.TSCreateText(MyFont, td);
	//-----------------------------synch with render thread------------------------------ 
	SetEvent(PhysicsEvent);
	WaitForSingleObject(RenderEvent,INFINITE);
}

void EndPhysics(DWORD ThreadID) {
	EventID = CreateEventW(NULL, FALSE, FALSE, NULL);
	PostThreadMessageW(ThreadID, WM_QUIT, NULL, NULL);
	WaitForSingleObject(EventID, INFINITE); //make sure the physics thread terminated before continuing
	CloseHandle(EventID);
	DeleteCriticalSection(&cs);
}

Offset* GetPositions(unsigned& count) {
	unsigned i = 0;
	EnterCriticalSection(&cs);
	Offset* mem = new Offset[BallObjects.size()];
	for (const auto& ball : BallObjects) {
		mem[i] = Offset{ ball.xposition , ball.yposition };
		i++;
	}
	LeaveCriticalSection(&cs);
	count = i;
	return mem;
}

static void DetectCollisions();
static void CollisionResponse();

DWORD PhysicsThread(LPVOID UnusedParam){
	LARGE_INTEGER frequency,begincount,currentcount;
	InitPhysics();
	QueryPerformanceFrequency(&frequency);
	while (1) {
		QueryPerformanceCounter(&begincount);
		EnterCriticalSection(&cs);

		//--------------------Input and Events--------------------------------------------------
		{
			MSG msg;
			float xaccum = 0.0f;
			float yaccum = 0.0f;
			while (PeekMessageW(&msg, NULL, NULL, NULL, PM_REMOVE)) {
				switch (msg.message) {
				case WM_SIZE: {
					WindowWidth = LOWORD(msg.lParam);
					WindowHeight = HIWORD(msg.lParam);
					break;
				}
				case WM_MOVE: {
					xaccum =+ (float)GET_X_LPARAM(msg.lParam) - XWindowPos;
					yaccum =+ (float)GET_Y_LPARAM(msg.lParam) - YWindowPos;
					break;
				}
				case WM_LBUTTONDOWN: {
					float Mousex = (float)GET_X_LPARAM(msg.lParam) - WindowWidth / 2.0f;
					float Mousey = -(float)GET_Y_LPARAM(msg.lParam) + WindowHeight / 2.0f;
					for (unsigned i = (unsigned)(BallObjects.size() - 1); i < BallObjects.size(); i--) {
						//The reason we go through the list in reverse is because objects later in the list appear in front
						//and we want to prioritize the objects in front
						BallObject& ball = BallObjects[i];
						float xdist = Mousex - ball.xposition;
						float ydist = Mousey - ball.yposition;
						if (xdist * xdist + ydist * ydist < BALLRADIUS * GrabRadiusMultiplier * BALLRADIUS * GrabRadiusMultiplier) { //mouse is clicking on a ball object
							GrabbedObject = i;
							break;
						}
					}
					break;
				}
				case WM_LBUTTONUP: {
					GrabbedObject = 0xffffffffu; //well run out of memory way before we have this many objects
					break;
				}
				case WM_MOUSEMOVE: {
					Mousex = (float)GET_X_LPARAM(msg.lParam) - WindowWidth / 2.0f;
					Mousey = -(float)GET_Y_LPARAM(msg.lParam) + WindowHeight / 2.0f;
					break;
				}
				case WM_KEYDOWN: {
					break;
				}
				case WM_QUIT: {
					LeaveCriticalSection(&cs); //before set and wait to avoid deadlock
					
					SetEvent(PhysicsEvent);
					WaitForSingleObject(RenderEvent, INFINITE);
					goto exit;
				}
				}
			}

			XWindowPos += xaccum;
			YWindowPos += yaccum;
			XWindowVelocity = xaccum * Rate;
			YWindowVelocity = -yaccum * Rate; //in window coords, yaccum increases downwards
			for (auto& Ball : BallObjects) {
				Ball.xposition -= xaccum;
				Ball.yposition += yaccum; //remember that yaccum increases downwards
			}

			if (GetKeyState('A') & 0x8000) {//if A is Pressed
				BallObjects.push_back(BallObject{ Mousex,Mousey, (rand() % 256) - 128.0f , (float)(rand() % 1024) });
			}
			if (GetKeyState('D') & 0x8000) {//if B is Pressed
				BallObjects.clear();
			}
			if (GetKeyState('S') & 0x8000) {//if B is Pressed
				if (!BallObjects.empty()) {
					BallObjects.pop_back();
				}
			}
		}
		//------------------------------------------Collision Detection--------------------------------------
		DetectCollisions();
		//------------------------------------------Collision Response--------------------------------------
		CollisionResponse();

		LeaveCriticalSection(&cs);
		do {
			QueryPerformanceCounter(&currentcount);
		} while ((currentcount.QuadPart - begincount.QuadPart) * Rate <= frequency.QuadPart);
		SetDebugInfo("framerate", std::to_string((double)frequency.QuadPart / (double)(currentcount.QuadPart - begincount.QuadPart)));
	}
exit:
	return 0;
}

static void DetectCollisions() {
	CollisionPoints.clear();
	for (unsigned i = 0; i < BallObjects.size(); i++) {
		BallObject& Ball1 = BallObjects[i];
		for (unsigned j = i + 1; j < BallObjects.size(); j++) {
			BallObject& Ball2 = BallObjects[j];
			float xdiff = Ball1.xposition - Ball2.xposition;
			float ydiff = Ball1.yposition - Ball2.yposition;
			float dist_2 = xdiff * xdiff + ydiff * ydiff; //distance squared
			if (dist_2 <= 4 * BALLRADIUS * BALLRADIUS) { //if distance^2 < (radius + radius)^2
				CollisionPoint cp;
				float dist = sqrt(dist_2);
				cp.object1 = i;
				cp.object2 = j;
				cp.xposition = (Ball1.xposition + Ball2.xposition) / 2.0f; //The collision point is just between the two colliding balls
				cp.yposition = (Ball1.yposition + Ball2.yposition) / 2.0f;
				cp.xnormal = xdiff / dist;
				cp.ynormal = ydiff / dist;
				CollisionPoints.push_back(cp);
			}
		}


		//Bottom edge
		if ((Ball1.yposition - BALLRADIUS) < (-WindowHeight / 2.0f)) {
			CollisionPoint cp;
			cp.xposition = Ball1.xposition;
			cp.yposition = ((Ball1.yposition - BALLRADIUS) + (-WindowHeight / 2.0f)) / 2.0f; //an average
			cp.object1 = i;
			cp.object2 = WINDOW_BOTTOM_ID;
			cp.xnormal = 0.0f;
			cp.ynormal = 1.0f;
			CollisionPoints.push_back(cp);
		}

		//left edge
		if ((Ball1.xposition - BALLRADIUS) < (-WindowWidth / 2.0f)) {
			CollisionPoint cp;
			cp.xposition = ((Ball1.xposition - BALLRADIUS) + (-WindowWidth / 2.0f)) / 2.0f; //an average;
			cp.yposition = Ball1.yposition;
			cp.object1 = i;
			cp.object2 = WINDOW_LEFT_ID;
			cp.xnormal = 1.0f;
			cp.ynormal = 0.0f;
			CollisionPoints.push_back(cp);
		}

		//right edge
		if ((Ball1.xposition + BALLRADIUS) > (WindowWidth / 2.0f)) {
			CollisionPoint cp;
			cp.xposition = ((Ball1.xposition + BALLRADIUS) + (WindowWidth / 2.0f)) / 2.0f; //an average;
			cp.yposition = Ball1.yposition;
			cp.object1 = i;
			cp.object2 = WINDOW_RIGHT_ID;
			cp.xnormal = -1.0f;
			cp.ynormal = 0.0f;
			CollisionPoints.push_back(cp);
		}

		//top edge
		if ((Ball1.yposition + BALLRADIUS) > (WindowHeight / 2.0f)) {
			CollisionPoint cp;
			cp.xposition = Ball1.xposition;
			cp.yposition = ((Ball1.yposition + BALLRADIUS) + (WindowHeight / 2.0f)) / 2.0f; //an average;
			cp.object1 = i;
			cp.object2 = WINDOW_TOP_ID;
			cp.xnormal = 0.0f;
			cp.ynormal = -1.0f;
			CollisionPoints.push_back(cp);
		}
	}
}

//I guess this function is a misnomer cuz it also do the movement of the objects while there are no collisions
static void CollisionResponse() {
	//-----------------------Apply non contact impulses/forces/velocity changes
	for (unsigned i = 0; i < BallObjects.size(); i++) {
		BallObject& ball = BallObjects[i];
		ball.yvelocity -= Gravity / Rate;

		if (i == GrabbedObject) { //The dragging behavior
			float xdist = Mousex - ball.xposition;
			float ydist = Mousey - ball.yposition;
			ball.xvelocity += (xdist * GrabForce) / Rate;
			ball.yvelocity += (ydist * GrabForce) / Rate;
		}
	}
	//-----------------------Resolve Collisions------------------------------------
	for (unsigned char iter = 0; iter < Iterations; iter++) {
		for (unsigned i = 0; i < CollisionPoints.size(); i++) {
			CollisionPoint& cp = CollisionPoints[i];
			BallObject& Object1 = BallObjects[cp.object1]; //Object 1 is always a ball
			float vel1 = Object1.xvelocity * cp.xnormal + Object1.yvelocity * cp.ynormal;
			//Object 1 is always a ball
			if (cp.object2 < WINDOW_LEFT_ID) { //if object2 is a ball
				BallObject& Object2 = BallObjects[cp.object2];
				float vel2 = Object2.xvelocity * cp.xnormal + Object2.yvelocity * cp.ynormal;
				float RelativeVelocity = vel1 - vel2;
				if (RelativeVelocity < 0) {//Object moving towards each other
					float impulse = 0.5f * BallMass * -RelativeVelocity * (Elasticity + 1);
					Object1.xvelocity += impulse / BallMass * cp.xnormal;
					Object1.yvelocity += impulse / BallMass * cp.ynormal;
					Object2.xvelocity -= impulse / BallMass * cp.xnormal;
					Object2.yvelocity -= impulse / BallMass * cp.ynormal;
				}
			}
			else /*Object2 is a wall*/ {
				float vel2 = XWindowVelocity * cp.xnormal + YWindowVelocity * cp.ynormal;
				float RelativeVelocity = vel1 - vel2;
				if (RelativeVelocity < 0) {//Object moving towards each other
					float impulse = BallMass * -RelativeVelocity * (Elasticity + 1);
					Object1.xvelocity += impulse / BallMass * cp.xnormal;
					Object1.yvelocity += impulse / BallMass * cp.ynormal;
				}
			}
		}
	}
	//-----------------------Apply Final Impulses---------------------------------
	for (auto& ball : BallObjects) {
		ball.xvelocity *= Damping;
		ball.yvelocity *= Damping;
		ball.xposition += ball.xvelocity / Rate;
		ball.yposition += ball.yvelocity / Rate;
	}
	//-----------------------Fix Overlaps-----------------------------------------
	for (unsigned char iter = 0; iter < FixIterations; iter++) {
		for (unsigned i = 0; i < CollisionPoints.size(); i++) {
			CollisionPoint& cp = CollisionPoints[i];
			BallObject& Object1 = BallObjects[cp.object1]; //Object 1 is always a ball
			//Object 1 is always a ball
			if (cp.object2 < WINDOW_LEFT_ID) { //if object2 is a ball
				BallObject& Object2 = BallObjects[cp.object2];
				float xdist = (Object1.xposition - cp.xposition) * cp.xnormal;
				float ydist = (Object1.yposition - cp.yposition) * cp.ynormal;
				float offset = BALLRADIUS - (xdist + ydist);
				if (offset > 0.0f) {
					Object1.xposition += offset * cp.xnormal * FixIterationsFactor;
					Object1.yposition += offset * cp.ynormal * FixIterationsFactor;
					Object2.xposition -= offset * cp.xnormal * FixIterationsFactor;
					Object2.yposition -= offset * cp.ynormal * FixIterationsFactor;
				}
			}
			else /*Object2 is a wall*/ {
				float xdist = (Object1.xposition - cp.xposition) * cp.xnormal;
				float ydist = (Object1.yposition - cp.yposition) * cp.ynormal;
				float offset = BALLRADIUS - (xdist + ydist);
				if (offset > 0.0f) {
					Object1.xposition += offset * cp.xnormal * FixIterationsFactor;
					Object1.yposition += offset * cp.ynormal * FixIterationsFactor;
				}
			}
		}
		DetectCollisions();		
	}
}