#include <windows.h>
#include <string>
#include <objidl.h>   
#include <gdiplus.h>
#include "EuroScopePlugIn.h"
#include "TopSkyFunctions.h"
#include "S-Mode.h"
#include "IndraApcInterop.h"
#include <map>

#include <iostream>
#include <string>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(linker, "/EXPORT:EuroScopePlugInInit=_EuroScopePlugInInit")
#pragma comment(linker, "/EXPORT:EuroScopePlugInExit=_EuroScopePlugInExit")

using namespace EuroScopePlugIn;
using namespace Gdiplus;
using namespace std;

void FillRoundedRect(Graphics& g, Brush& brush, int x, int y, int w, int h, int radius)
{
    GraphicsPath path;
    path.AddArc(x,             y,              radius * 2, radius * 2, 180, 90); // top-left
    path.AddArc(x + w - radius * 2, y,              radius * 2, radius * 2, 270, 90); // top-right
    path.AddArc(x + w - radius * 2, y + h - radius * 2, radius * 2, radius * 2,   0, 90); // bottom-right
    path.AddArc(x,             y + h - radius * 2, radius * 2, radius * 2,  90, 90); // bottom-left
    path.CloseFigure();
    g.FillPath(&brush, &path);
}

void FindCorrectStar(char Airport, char Fix) {

}

HINSTANCE g_hDllInstance = nullptr;
ULONG_PTR g_gdiplusToken  = 0;
int Auto_Star = 0;
int Coordination_cross = 0;
int FMP_Panel = 0;
int S_mode = 0;
int Indra_Panel = 1;

int Indra_sent_drawn = 0;
int Indra_sent_not_drawn = 0;

const int OBJECT_AUTOSTAR = 2;  
const int OBJECT_COORDINATION_CROSS = 3;
const int OBJECT_FMP_PANEL = 4;
const int OBJECT_S_MODE_PANEL = 5;
const int OBJECT_INDRA_PANEL = 6;

const int STAR_TAG_ID     = 100;
const int STAR_TAG_FUNCTION = 101;

std::map<std::string, int> starState;


BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_hDllInstance = hModule;

        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        GdiplusShutdown(g_gdiplusToken);
    }
    return TRUE;
}

const int OBJECT_CIRCLE = 10;

class MyRadarScreen : public CRadarScreen
{
    Gdiplus::Image* m_pImage = nullptr;
    int m_X = -1;
    int m_Y = -1;
    int m_Set_draw = 0;
    int m_traffic_load = 0;
    int m_indra_draw = 1;
public:
    MyRadarScreen()
    {
        // Build path: <DLL folder>\image.png
        wchar_t dllPath[MAX_PATH] = {};
        GetModuleFileNameW(g_hDllInstance, dllPath, MAX_PATH);

        wchar_t* lastSlash = wcsrchr(dllPath, L'\\');
        if (lastSlash)
            *(lastSlash + 1) = L'\0';
        wcscat_s(dllPath, L"image.png");

        m_pImage = Gdiplus::Image::FromFile(dllPath);

        // If load failed, set to nullptr
        if (m_pImage && m_pImage->GetLastStatus() != Ok)
        {
            delete m_pImage;
            m_pImage = nullptr;
        }

    }

    ~MyRadarScreen()
    {
        delete m_pImage;
    }

        virtual void OnRefresh(HDC hDC, int Phase) override
        {
            if (Phase != REFRESH_PHASE_AFTER_TAGS)
                return;

            if (!m_pImage)
                return;

            int w = (int)m_pImage->GetWidth();
            int h = (int)m_pImage->GetHeight();

            if (m_X == -1)
            {
                RECT radarArea = GetRadarArea();
                m_X = radarArea.right - w / 2 - 10;
                m_Y = (radarArea.top + 200)  + h / 2 + 10;
            }

        Graphics graphics(hDC);
        graphics.DrawImage(m_pImage, m_X - w / 2, m_Y - h / 2, w, h);

        RECT imgRect = {
            m_X - w / 2,
            m_Y - h / 2,
            m_X + w / 2,
            m_Y + h / 2
        };
        //Image click spot
        AddScreenObject(OBJECT_CIRCLE, "MyImage", imgRect, true, "");
        if (m_Set_draw)
        {
        
        Pen pen(Color(255, 255,255,255), 2.0f);  
        int panelX      = imgRect.left - 200;
        int panelY      = imgRect.top;
        int panelWidth  = 200;
        int panelHeight = imgRect.bottom - imgRect.top + 200; 


        // Grey settings window
        SolidBrush greyBrush(Color(210, 50,50,50));
        FillRoundedRect(graphics, greyBrush, panelX, panelY, panelWidth, panelHeight, 4);


        // Settings Text
        SolidBrush textBrush(Color(255, 255,255,255));
        FontFamily fontFamily(L"EuroScope");
        Font font(&fontFamily, 18, FontStyleRegular, UnitPixel);
        PointF textPoint(panelX + 5, panelY + 5);
        graphics.DrawString(L"Settings", -1, &font, textPoint, &textBrush);

        //Click Spots
        RECT autoStarRect = {
            panelX + 10,
            panelY + 40,
            panelX + 10 + 200,  // approximate width of the text
            panelY + 40 + 14    // approximate height (font size)
        };
        RECT CoordinationCrossRect = {
            panelX + 10,
            panelY + 80,
            panelX + 10 + 200,  // approximate width of the text
            panelY + 80 + 14    // approximate height (font size)
        };
        RECT FMP_PanelRect = {
            panelX + 10,
            panelY + 120,
            panelX + 10 + 200,  // approximate width of the text
            panelY + 120 + 14    // approximate height (font size)
        };
        RECT Indra_PanelRect = {
            panelX + 10,
            panelY + 160,
            panelX + 10 + 200,  // approximate width of the text
            panelY + 160 + 14    // approximate height (font size)
        };
        

        if (S_mode)
        {
            SMode::DrawAllIndicators(hDC, GetPlugIn(), this);
        }
        AddScreenObject(OBJECT_AUTOSTAR, "AutoStar", autoStarRect, false, "Toggle Auto Star");
        AddScreenObject(OBJECT_COORDINATION_CROSS, "CoordCross", CoordinationCrossRect, false, "Toggle S-Mode");
        AddScreenObject(OBJECT_FMP_PANEL, "FMPPanel", FMP_PanelRect, false, "Toggle FMP Panel");
        AddScreenObject(OBJECT_INDRA_PANEL, "IndraPanel", Indra_PanelRect, false, "Toggle Indra Panel");

        //Auto Star
        //Auto Star Box
        SolidBrush AutoStarColour(Color(235, 64,64,69));
        FillRoundedRect(graphics, AutoStarColour, panelX+5, panelY+35.5, panelWidth-15, 25, 6);

        //Auto Star Text
        SolidBrush textBrush2(Color(255, 180, 184, 181));
        FontFamily fontFamily1(L"EuroScope");
        Font font2(&fontFamily1, 12, FontStyleRegular, UnitPixel);
        PointF textPoint2(panelX + 10, panelY + 40);
        graphics.DrawString(L"Auto assign STAR", -1, &font2, textPoint2, &textBrush2);
        if (Auto_Star)
        {
            SolidBrush AutoStarOnColour(Color(235, 0,200,0));
            graphics.FillRectangle(&AutoStarOnColour, panelX+160, panelY+44.5, 7.5, 7.5);
        }
        else
        {
            SolidBrush AutoStarOffColour(Color(235, 250, 67, 67));
            graphics.FillRectangle(&AutoStarOffColour, panelX+160, panelY+44.5, 7.5, 7.5);
        }

        //Coordination Cross
        //Coordination Cross Box 
        SolidBrush CoordinationColour(Color(235, 64,64,69));
        FillRoundedRect(graphics, CoordinationColour, panelX+5, panelY+75.5, panelWidth-15, 25, 6);
        //Coordination Cross Text
        PointF textPoint3(panelX + 10, panelY + 80);
        graphics.DrawString(L"S-Mode", -1, &font2, textPoint3, &textBrush2);
        if (S_mode)       
        {
            SolidBrush CoordOnColour(Color(235, 0,200,0));
            graphics.FillRectangle(&CoordOnColour, panelX+160, panelY+84.5, 7.5, 7.5);
        }
        else
        {
            SolidBrush CoordOffColour(Color(235, 250, 67, 67));
            graphics.FillRectangle(&CoordOffColour, panelX+160, panelY+84.5, 7.5, 7.5);
        }

        //FMP Panel
        //FMP Panel Box
        SolidBrush FMPColour(Color(235, 64,64,69));
        FillRoundedRect(graphics, FMPColour, panelX+5, panelY+115.5, panelWidth-15, 25, 6);

        //FMP Panel Text
        PointF textPoint4(panelX + 10, panelY + 120);
        graphics.DrawString(L"FMP Panel", -1, &font2, textPoint4, &textBrush2);
        if (FMP_Panel)
        {
            SolidBrush FMPOnColour(Color(235, 0,200,0));
            graphics.FillRectangle(&FMPOnColour, panelX+160, panelY+124.5, 7.5, 7.5);
        }
        else
        {
            SolidBrush FMPOffColour(Color(235, 250, 67, 67));
            graphics.FillRectangle(&FMPOffColour, panelX+160, panelY+124.5, 7.5, 7.5);
        }
        //Indra Panel
        //Indra Panel Box
        SolidBrush IndraColour(Color(235, 64,64,69));
        FillRoundedRect(graphics, IndraColour, panelX+5, panelY+155.5, panelWidth-15, 25, 6);

        //Indra Panel Text
        PointF textPoint5(panelX + 10, panelY + 160);
        graphics.DrawString(L"Indra Panel", -1, &font2, textPoint5, &textBrush2);
        if (Indra_Panel)
        {
            SolidBrush IndraOnColour(Color(235, 0,200,0));
            graphics.FillRectangle(&IndraOnColour, panelX+160, panelY+164.5, 7.5, 7.5);
        }
        else
        {
            SolidBrush IndraOffColour(Color(235, 250, 67, 67));
            graphics.FillRectangle(&IndraOffColour, panelX+160, panelY+164.5, 7.5, 7.5);
        }
    }
}
        // Defines button clicks
        virtual void OnClickScreenObject(
            int ObjectType,
            const char* sObjectId,
            POINT Pt,
            RECT Area,
            int Button) override
        {
            if (ObjectType == OBJECT_CIRCLE && Button == BUTTON_LEFT)
            {
                m_Set_draw = 1 - m_Set_draw;
                RequestRefresh();
            }

            if (ObjectType == OBJECT_CIRCLE && Button == BUTTON_RIGHT)
            {
                        CRadarTarget rt = GetPlugIn()->RadarTargetSelectASEL();
                        if (rt.IsValid())
                        {
                        StartTagFunction(
                            rt.GetCallsign(),
                            "TopSky plugin",  
                            0,
                            "",
                            "TopSky plugin",   
                            TopSky::TOGGLE_ROUTE_DRAW_MTCD_SAP,
                            Pt,
                            Area
                            );
    
                        }
                RequestRefresh();
            }

            if (ObjectType == OBJECT_CIRCLE && Button == BUTTON_MIDDLE)
            {
                m_Set_draw = 1 - m_Set_draw;
                RequestRefresh();
            }

            if (ObjectType == OBJECT_AUTOSTAR && Button == BUTTON_LEFT)
            {
                Auto_Star = 1 - Auto_Star;
                RequestRefresh();
            }

            if (ObjectType == OBJECT_AUTOSTAR && Button == BUTTON_RIGHT)
            {
                Auto_Star = 1;
                RequestRefresh();
            }
            if (ObjectType == OBJECT_COORDINATION_CROSS && Button == BUTTON_LEFT)
            {
                S_mode = 1 - S_mode;
                RequestRefresh();
            }

            if (ObjectType == OBJECT_COORDINATION_CROSS && Button == BUTTON_RIGHT)
            {
                S_mode = 1;
                RequestRefresh();
            }
            if (ObjectType == OBJECT_FMP_PANEL && Button == BUTTON_LEFT)
            {
                FMP_Panel = 1 - FMP_Panel;
                RequestRefresh();
            }

            if (ObjectType == OBJECT_FMP_PANEL && Button == BUTTON_RIGHT)
            {
                FMP_Panel = 1;
                RequestRefresh();
            }
            if (ObjectType == OBJECT_INDRA_PANEL && Button == BUTTON_LEFT)
            {
                Indra_Panel = 1 - Indra_Panel;
                RequestRefresh();
            }

        }

    virtual void OnMoveScreenObject(
        int ObjectType,
        const char* sObjectId,
        POINT Pt,
        RECT Area,
        bool Released) override
    {
        if (ObjectType != OBJECT_CIRCLE)
            return;

        // Pt is the current mouse position while dragging
        m_X = Pt.x;
        m_Y = Pt.y;

        RequestRefresh();
    }

    virtual void OnAsrContentToBeClosed() override {}
};

class MyPlugIn : public CPlugIn
{
    MyRadarScreen* m_Screen = nullptr;
public:
    MyPlugIn()
        : CPlugIn(
            COMPATIBILITY_CODE,
            "Mag Plugin",
            "0.0.1",
            "Maghreb vACC",
            "GPL V3"
        )
    {
        RegisterTagItemType("Star TAG Display", STAR_TAG_ID);
        RegisterTagItemFunction("STAR Click", STAR_TAG_FUNCTION);

        DisplayUserMessage(
            "Maghreb vACC",
            "Mag Plugin",
            "Plugin loaded successfully",
            true, true, false, false, false
        );
    }
    
    virtual void OnFunctionCall(
    int FunctionId,
    const char* sItemString,
    POINT Pt,
    RECT Area) override
{
    if (FunctionId == STAR_TAG_FUNCTION)
    {
        CRadarTarget rt = RadarTargetSelectASEL(); // get ASEL target instead
        if (!rt.IsValid())
            return;

        std::string callsign = rt.GetCallsign();
        starState[callsign] = (starState[callsign] + 1) % 3; 
    }
}

    virtual void OnGetTagItem(
        CFlightPlan FlightPlan,
        CRadarTarget rt,
        int ItemCode,
        int TagData,
        char sItemString[16],
        int* pColorCode,
        COLORREF* pRgbColor,
        double* pFontSize) override {
        if (ItemCode == STAR_TAG_ID)
        {
            if (!rt.IsValid())
                return;
            std::string callsign = rt.GetCallsign();
            CFlightPlanData fpData = FlightPlan.GetFlightPlanData();
            CSectorElement secElm = CSectorElement();

            std::string StarName = fpData.GetStarName();
            std::string airport = fpData.GetDestination();

            std::string runway = FlightPlan.GetFlightPlanData().GetArrivalRwy();

            std::string new_star = StarName.substr(0, 5);

            strncpy_s(sItemString, 16, new_star.c_str(), _TRUNCATE);

            int state = 0; 

            auto it = starState.find(callsign);
            if (it != starState.end())
                state = it->second;

            *pColorCode = TAG_COLOR_RGB_DEFINED;

            switch (state)
            {
            case 0:
                *pRgbColor = RGB(255, 92, 103); // red
                break;

            case 1:
                *pRgbColor = RGB(0, 255, 0);    // green
                break;

            case 2:
                *pRgbColor = RGB(255, 255, 0);  // yellow
                break;
            }
        }
    }
    virtual CRadarScreen* OnRadarScreenCreated(
        const char* sDisplayName,
        bool NeedRadarContent,
        bool GeoReferenced,
        bool CanBeSaved,
        bool CanBeCreated) override
    {
        m_Screen = new MyRadarScreen();
        return m_Screen;
    }

    virtual void OnRadarTargetPositionUpdate(CRadarTarget rt) override
    {
        if (!rt.IsValid())
            return;

        const char* callsign = rt.GetCallsign();
        if (!callsign)
            return;

        CFlightPlan fp = rt.GetCorrelatedFlightPlan();
        if (!fp.IsValid())
            return;

        if (m_Screen)
            m_Screen->RequestRefresh();
    }

    virtual ~MyPlugIn() {}
};

MyPlugIn* gPlugin = nullptr;

__declspec(dllexport) void EuroScopePlugInInit(CPlugIn** ppPlugIn)
{
    gPlugin = new MyPlugIn();
    *ppPlugIn = gPlugin;
}
 
__declspec(dllexport) void EuroScopePlugInExit()
{
}