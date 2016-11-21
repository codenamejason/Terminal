/********************************************************
*                                                       *
*   Copyright (C) Microsoft. All rights reserved.       *
*                                                       *
********************************************************/

#include <precomp.h>
#include <wextestclass.h>
#include "..\..\inc\consoletaeftemplates.hpp"

#include "MouseInput.hpp"
using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace Microsoft
{
    namespace Console
    {
        namespace VirtualTerminal
        {
            class MouseInputTest;
        };
    };
};
using namespace Microsoft::Console::VirtualTerminal;

// For magic reasons, this has to live outside the class. Something wonderful about TAEF macros makes it
// invisible to the linker when inside the class.
static wchar_t* s_pwszInputExpected;

static wchar_t s_pwszExpectedBuffer[BYTE_MAX]; // big enough for anything

static COORD s_rgTestCoords [] = {
    {0, 0},
    {0, 1},
    {1, 1},
    {2, 2},
    {94, 94}, // 94+1+32 = 127
    {95, 95}, // 95+1+32 = 128, this is the ascii boundary
    {96, 96},
    {127,127},
    {128,128},
    {SHORT_MAX-33,SHORT_MAX-33}, 
    {SHORT_MAX-32,SHORT_MAX-32},
};

// Note: We're going to be changing the value of the third char (the space) of 
//      these strings as we test things with this array, to alter the expected button value.
// The default value is the button=WM_LBUTTONDOWN case, which is element[3]=' ' 
static wchar_t* s_rgDefaultTestOutput [] = {
    L"\x1b[M !!",
    L"\x1b[M !\"",
    L"\x1b[M \"\"",
    L"\x1b[M ##",
    L"\x1b[M \x7f\x7f",
    L"\x1b[M \x80\x80", // 95 - This and below will always fail for default (non utf8)
    L"\x1b[M \x81\x81",
    L"\x1b[M \x00A0\x00A0", //127
    L"\x1b[M \x00A1\x00A1",
    L"\x1b[M \x7FFF\x7FFF", // FFDE
    L"\x1b[M \x8000\x8000", // This one will always fail for Default and UTF8
};

// Note: We're going to be changing the value of the third char (the space) of 
//      these strings as we test things with this array, to alter the expected button value.
// The default value is the button=WM_LBUTTONDOWN case, which is element[3]='0' 
// We're also going to change the last element, for button-down (M) vs button-up (m)
static wchar_t* s_rgSgrTestOutput [] = {
    L"\x1b[<%d;1;1M",
    L"\x1b[<%d;1;2M",
    L"\x1b[<%d;2;2M",
    L"\x1b[<%d;3;3M",
    L"\x1b[<%d;95;95M",
    L"\x1b[<%d;96;96M", // 95 - This and below will always fail for default (non utf8)
    L"\x1b[<%d;97;97M",
    L"\x1b[<%d;128;128M", //127
    L"\x1b[<%d;129;129M",
    L"\x1b[<%d;32735;32735M", // FFDE
    L"\x1b[<%d;32736;32736M",
};

static int s_iTestCoordsLength = ARRAYSIZE(s_rgTestCoords);

class MouseInputTest
{
public:

    TEST_CLASS(MouseInputTest);

    static void s_MouseInputTestCallback(_In_reads_(cInput) INPUT_RECORD* rgInput, _In_ DWORD cInput)
    {
        Log::Comment(L"MouseInput successfully generated a sequence for the input, and sent it.");

        size_t cInputExpected = 0;
        VERIFY_SUCCEEDED(StringCchLengthW(s_pwszInputExpected, STRSAFE_MAX_CCH, &cInputExpected));

        if (VERIFY_ARE_EQUAL(cInputExpected, cInput, L"Verify expected and actual input array lengths matched."))
        {
            Log::Comment(L"We are expecting always key events and always key down. All other properties should not be written by simulated keys.");

            INPUT_RECORD irExpected = { 0 };
            irExpected.EventType = KEY_EVENT;
            irExpected.Event.KeyEvent.bKeyDown = TRUE;
            irExpected.Event.KeyEvent.wRepeatCount = 1;

            Log::Comment(L"Verifying individual array members...");
            for (size_t i = 0; i < cInput; i++)
            {
                irExpected.Event.KeyEvent.uChar.UnicodeChar = s_pwszInputExpected[i];
                VERIFY_ARE_EQUAL(irExpected, rgInput[i], 
                                 NoThrowString().Format(L"Chars='%c','%c'", s_pwszInputExpected[i], rgInput[i].Event.KeyEvent.uChar.UnicodeChar));
            }
        }
    }

    void ClearTestBuffer()
    {
        memset(s_pwszExpectedBuffer, 0, ARRAYSIZE(s_pwszExpectedBuffer) * sizeof(wchar_t));
    }

    // Routine Description:
    // Constructs a string from s_rgDefaultTestOutput with the third char
    //      correctly filled in to match uiButton.
    wchar_t* BuildDefaultTestOutput(wchar_t* pwchTestOutput, unsigned int uiButton, short sModifierKeystate, short sScrollDelta)
    {
        Log::Comment(NoThrowString().Format(L"Input Test Output:\'%s\'", pwchTestOutput));
        // Copy the expected output into the buffer
        size_t cchInputExpected = 0;
        VERIFY_SUCCEEDED(StringCchLengthW(pwchTestOutput, STRSAFE_MAX_CCH, &cchInputExpected));
        VERIFY_ARE_EQUAL(cchInputExpected, 6);
        
        ClearTestBuffer();
        memcpy(s_pwszExpectedBuffer, pwchTestOutput, cchInputExpected * sizeof(wchar_t));
        
        // Change the expected button value
        wchar_t wch = GetDefaultCharFromButton(uiButton, sModifierKeystate, sScrollDelta);
        Log::Comment(NoThrowString().Format(L"Button Char was:\'%d\' for uiButton '%d", (int)wch, uiButton));

        s_pwszExpectedBuffer[3] = wch;
        Log::Comment(NoThrowString().Format(L"Expected Input:\'%s\'", s_pwszExpectedBuffer));
        return s_pwszExpectedBuffer;
    }

    // Routine Description:
    // Constructs a string from s_rgSgrTestOutput with the third and last chars 
    //      correctly filled in to match uiButton.
    wchar_t* BuildSGRTestOutput(wchar_t* pwchTestOutput, unsigned int uiButton, short sModifierKeystate, short sScrollDelta)
    {

        ClearTestBuffer();

        // Copy the expected output into the buffer
        // TODO MSFT:9130937 - VS is content with calling `swprintf` here, but razzle doesn't like the `count` param
        swprintf_s(s_pwszExpectedBuffer, BYTE_MAX, pwchTestOutput, GetSgrCharFromButton(uiButton, sModifierKeystate, sScrollDelta));
        
        size_t cchInputExpected = 0;
        VERIFY_SUCCEEDED(StringCchLengthW(s_pwszExpectedBuffer, STRSAFE_MAX_CCH, &cchInputExpected));

        s_pwszExpectedBuffer[cchInputExpected-1] = IsButtonDown(uiButton)? L'M' : L'm';
        
        Log::Comment(NoThrowString().Format(L"Expected Input:\'%s\'", s_pwszExpectedBuffer));
        return s_pwszExpectedBuffer;
    }

    wchar_t GetDefaultCharFromButton(unsigned int uiButton, short sModifierKeystate, short sScrollDelta)
    {
        wchar_t wch = L'\x0';
        Log::Comment(NoThrowString().Format(L"uiButton '%d'", uiButton));
        switch (uiButton)
        {
            case WM_LBUTTONDBLCLK:
            case WM_LBUTTONDOWN:
                wch = L' ';
                break;
            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP:
                wch = L'#';
                break;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONDBLCLK:
                wch = L'\"';
                break;
            case WM_MBUTTONDOWN:
            case WM_MBUTTONDBLCLK:
                wch = L'!';
                break;
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
                Log::Comment(NoThrowString().Format(L"MOUSEWHEEL"));
                wch = L'`' + (sScrollDelta > 0? 0 : 1);
                break;
            case WM_MOUSEMOVE:
            default:
                Log::Comment(NoThrowString().Format(L"DEFAULT"));
                wch = L'\x0';
                break;
        }
        // MK_SHIFT is ignored by the translator
        wch += (sModifierKeystate & MK_CONTROL) ? 0x08 : 0x00;
        return wch;
    }

    wchar_t GetSgrCharFromButton(unsigned int uiButton, short sModifierKeystate, short sScrollDelta)
    {
        wchar_t wch = L'\x0';
        switch (uiButton)
        {
            case WM_LBUTTONDBLCLK:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONDBLCLK:
                wch = GetDefaultCharFromButton(uiButton, 0, sScrollDelta) - ' ';
                break;
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
                wch = 64 + (sScrollDelta > 0? 0 : 1);
                break;
            case WM_MOUSEMOVE:
            default:
                wch = L'\x0';
                break;
        }
        // MK_SHIFT is ignored by the translator
        wch += (sModifierKeystate & MK_CONTROL) ? 0x08 : 0x00;
        return wch;
    }

    bool IsButtonDown(unsigned int uiButton)
    {
        bool fIsDown = false;
        switch (uiButton)
        {
            case WM_LBUTTONDBLCLK:
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONDBLCLK:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
                fIsDown = true;
                break;
        }
        return fIsDown;
    }

    /* From winuser.h - Needed to manually specify the test properties
        #define WM_MOUSEFIRST                   0x0200
        #define WM_MOUSEMOVE                    0x0200
        #define WM_LBUTTONDOWN                  0x0201
        #define WM_LBUTTONUP                    0x0202
        #define WM_LBUTTONDBLCLK                0x0203
        #define WM_RBUTTONDOWN                  0x0204
        #define WM_RBUTTONUP                    0x0205
        #define WM_RBUTTONDBLCLK                0x0206
        #define WM_MBUTTONDOWN                  0x0207
        #define WM_MBUTTONUP                    0x0208
        #define WM_MBUTTONDBLCLK                0x0209
        #if (_WIN32_WINNT >= 0x0400) || (_WIN32_WINDOWS > 0x0400)
        #define WM_MOUSEWHEEL                   0x020A
        #endif
        #if (_WIN32_WINNT >= 0x0500)
        #define WM_XBUTTONDOWN                  0x020B
        #define WM_XBUTTONUP                    0x020C
        #define WM_XBUTTONDBLCLK                0x020D
        #endif
        #if (_WIN32_WINNT >= 0x0600)
        #define WM_MOUSEHWHEEL                  0x020E
    */
    TEST_METHOD(DefaultModeTests)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            // TEST_METHOD_PROPERTY(L"Data:uiButton", L"{WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MBUTTONDOWN, WM_MBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP}") 
            TEST_METHOD_PROPERTY(L"Data:uiButton", L"{0x0201, 0x0202, 0x0207, 0x0208, 0x0204, 0x0205}") 
            // None, MK_SHIFT, MK_CONTROL
            TEST_METHOD_PROPERTY(L"Data:uiModifierKeystate", L"{0x0000, 0x0004, 0x0008}") 
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");

        MouseInput* const pInput = new MouseInput(s_MouseInputTestCallback);

        unsigned int uiModifierKeystate = 0;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiModifierKeystate", uiModifierKeystate));
        short sModifierKeystate = (SHORT)uiModifierKeystate;
        short sScrollDelta = 0;

        unsigned int uiButton;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiButton", uiButton));

        bool fExpectedKeyHandled = false;
        s_pwszInputExpected = L"\x0";
        VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse({0,0}, uiButton, sModifierKeystate, sScrollDelta));

        pInput->EnableDefaultTracking(true);

        for (int i = 0; i < s_iTestCoordsLength; i++)
        {
            COORD Coord = s_rgTestCoords[i];
            fExpectedKeyHandled = (Coord.X <= 94 && Coord.Y <= 94);
            Log::Comment(NoThrowString().Format(L"fHandled, x, y = (%d, %d, %d)", fExpectedKeyHandled, Coord.X, Coord.Y));

            s_pwszInputExpected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);

            // validate translation
            VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse(Coord, uiButton, sModifierKeystate, sScrollDelta),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.X, Coord.Y));
        }

        pInput->EnableButtonEventTracking(true);
        for (int i = 0; i < s_iTestCoordsLength; i++)
        {
            COORD Coord = s_rgTestCoords[i];
            fExpectedKeyHandled = (Coord.X <= 94 && Coord.Y <= 94);
            Log::Comment(NoThrowString().Format(L"fHandled, x, y = (%d, %d, %d)", fExpectedKeyHandled, Coord.X, Coord.Y));

            s_pwszInputExpected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);

            // validate translation
            VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse(Coord, uiButton, sModifierKeystate, sScrollDelta),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.X, Coord.Y));
        }

        pInput->EnableAnyEventTracking(true);
        for (int i = 0; i < s_iTestCoordsLength; i++)
        {
            COORD Coord = s_rgTestCoords[i];
            fExpectedKeyHandled = (Coord.X <= 94 && Coord.Y <= 94);
            Log::Comment(NoThrowString().Format(L"fHandled, x, y = (%d, %d, %d)", fExpectedKeyHandled, Coord.X, Coord.Y));

            s_pwszInputExpected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);

            // validate translation
            VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse(Coord, uiButton, sModifierKeystate, sScrollDelta),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.X, Coord.Y));
        }

    }
    TEST_METHOD(Utf8ModeTests)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            // TEST_METHOD_PROPERTY(L"Data:uiButton", L"{WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MBUTTONDOWN, WM_MBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP}") 
            TEST_METHOD_PROPERTY(L"Data:uiButton", L"{0x0201, 0x0202, 0x0207, 0x0208, 0x0204, 0x0205}") 
            TEST_METHOD_PROPERTY(L"Data:uiModifierKeystate", L"{0x0000, 0x0004, 0x0008}") 
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");

        MouseInput* const pInput = new MouseInput(s_MouseInputTestCallback);

        unsigned int uiModifierKeystate = 0;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiModifierKeystate", uiModifierKeystate));
        short sModifierKeystate = (SHORT)uiModifierKeystate;
        short sScrollDelta = 0;

        unsigned int uiButton;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiButton", uiButton));

        bool fExpectedKeyHandled = false;
        s_pwszInputExpected = L"\x0";
        VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse({0,0}, uiButton, sModifierKeystate, sScrollDelta));

        pInput->SetUtf8ExtendedMode(true);

        short MaxCoord = SHORT_MAX - 33;

        pInput->EnableDefaultTracking(true);
        for (int i = 0; i < s_iTestCoordsLength; i++)
        {
            COORD Coord = s_rgTestCoords[i];

            fExpectedKeyHandled = (Coord.X <= MaxCoord && Coord.Y <= MaxCoord);
            Log::Comment(NoThrowString().Format(L"fHandled, x, y = (%d, %d, %d)", fExpectedKeyHandled, Coord.X, Coord.Y));
            s_pwszInputExpected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);

            // validate translation
            VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse(Coord, uiButton, sModifierKeystate, sScrollDelta),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.X, Coord.Y));

        }

        pInput->EnableButtonEventTracking(true);
        for (int i = 0; i < s_iTestCoordsLength; i++)
        {
            COORD Coord = s_rgTestCoords[i];

            fExpectedKeyHandled = (Coord.X <= MaxCoord && Coord.Y <= MaxCoord);
            Log::Comment(NoThrowString().Format(L"fHandled, x, y = (%d, %d, %d)", fExpectedKeyHandled, Coord.X, Coord.Y));
            s_pwszInputExpected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);

            // validate translation
            VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse(Coord, uiButton, sModifierKeystate, sScrollDelta),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.X, Coord.Y));

        }
        
        pInput->EnableAnyEventTracking(true);
        for (int i = 0; i < s_iTestCoordsLength; i++)
        {
            COORD Coord = s_rgTestCoords[i];

            fExpectedKeyHandled = (Coord.X <= MaxCoord && Coord.Y <= MaxCoord);
            Log::Comment(NoThrowString().Format(L"fHandled, x, y = (%d, %d, %d)", fExpectedKeyHandled, Coord.X, Coord.Y));
            s_pwszInputExpected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);

            // validate translation
            VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse(Coord, uiButton, sModifierKeystate, sScrollDelta),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.X, Coord.Y));

        }
    }
    TEST_METHOD(SgrModeTests)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            // TEST_METHOD_PROPERTY(L"Data:uiButton", L"{WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MBUTTONDOWN, WM_MBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP}") 
            TEST_METHOD_PROPERTY(L"Data:uiButton", L"{0x0201, 0x0202, 0x0207, 0x0208, 0x0204, 0x0205}") 
            TEST_METHOD_PROPERTY(L"Data:uiModifierKeystate", L"{0x0000, 0x0004, 0x0008}") 
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");

        MouseInput* const pInput = new MouseInput(s_MouseInputTestCallback);
        unsigned int uiModifierKeystate = 0;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiModifierKeystate", uiModifierKeystate));
        short sModifierKeystate = (SHORT)uiModifierKeystate;
        short sScrollDelta = 0;

        unsigned int uiButton;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiButton", uiButton));

        bool fExpectedKeyHandled = false;
        s_pwszInputExpected = L"\x0";
        VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse({0,0}, uiButton, sModifierKeystate, sScrollDelta));

        pInput->SetSGRExtendedMode(true);
        fExpectedKeyHandled = true; // SGR Mode should be able to handle any arbitrary coords.

        pInput->EnableDefaultTracking(true);
        for (int i = 0; i < s_iTestCoordsLength; i++)
        {
            COORD Coord = s_rgTestCoords[i];

            Log::Comment(NoThrowString().Format(L"fHandled, x, y = (%d, %d, %d)", fExpectedKeyHandled, Coord.X, Coord.Y));
            s_pwszInputExpected = BuildSGRTestOutput(s_rgSgrTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);

            // validate translation
            VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse(Coord, uiButton, sModifierKeystate, sScrollDelta),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.X, Coord.Y));

        }

        pInput->EnableButtonEventTracking(true);
        for (int i = 0; i < s_iTestCoordsLength; i++)
        {
            COORD Coord = s_rgTestCoords[i];

            Log::Comment(NoThrowString().Format(L"fHandled, x, y = (%d, %d, %d)", fExpectedKeyHandled, Coord.X, Coord.Y));
            s_pwszInputExpected = BuildSGRTestOutput(s_rgSgrTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);

            // validate translation
            VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse(Coord, uiButton, sModifierKeystate, sScrollDelta),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.X, Coord.Y));

        }

        pInput->EnableAnyEventTracking(true);
        for (int i = 0; i < s_iTestCoordsLength; i++)
        {
            COORD Coord = s_rgTestCoords[i];

            Log::Comment(NoThrowString().Format(L"fHandled, x, y = (%d, %d, %d)", fExpectedKeyHandled, Coord.X, Coord.Y));
            s_pwszInputExpected = BuildSGRTestOutput(s_rgSgrTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);

            // validate translation
            VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse(Coord, uiButton, sModifierKeystate, sScrollDelta),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.X, Coord.Y));

        }
    }

    TEST_METHOD(ScrollWheelTests)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:sScrollDelta", L"{0, -1, 1, 100, -10000, 32736}") 
            TEST_METHOD_PROPERTY(L"Data:uiModifierKeystate", L"{0x0000, 0x0004, 0x0008}") 
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");

        MouseInput* const pInput = new MouseInput(s_MouseInputTestCallback);
        unsigned int uiModifierKeystate = 0;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiModifierKeystate", uiModifierKeystate));
        short sModifierKeystate = (SHORT)uiModifierKeystate;

        unsigned int uiButton = WM_MOUSEWHEEL;
        int iScrollDelta = 0;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"sScrollDelta", iScrollDelta));
        short sScrollDelta = (short)(iScrollDelta);

        bool fExpectedKeyHandled = false;
        s_pwszInputExpected = L"\x0";
        VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse({0,0}, uiButton, sModifierKeystate, sScrollDelta));

        // Default Tracking, Default Encoding
        pInput->EnableDefaultTracking(true);

        for (int i = 0; i < s_iTestCoordsLength; i++)
        {
            COORD Coord = s_rgTestCoords[i];
            fExpectedKeyHandled = (Coord.X <= 94 && Coord.Y <= 94);
            
            Log::Comment(NoThrowString().Format(L"fHandled, x, y = (%d, %d, %d)", fExpectedKeyHandled, Coord.X, Coord.Y));
            s_pwszInputExpected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);

            // validate translation
            VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse(Coord, uiButton, sModifierKeystate, sScrollDelta),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.X, Coord.Y));

        }

        // Default Tracking, UTF8 Encoding
        pInput->SetUtf8ExtendedMode(true);
        short MaxCoord = SHORT_MAX - 33;
        for (int i = 0; i < s_iTestCoordsLength; i++)
        {
            COORD Coord = s_rgTestCoords[i];
            fExpectedKeyHandled = (Coord.X <= MaxCoord && Coord.Y <= MaxCoord);
            
            Log::Comment(NoThrowString().Format(L"fHandled, x, y = (%d, %d, %d)", fExpectedKeyHandled, Coord.X, Coord.Y));
            s_pwszInputExpected = BuildDefaultTestOutput(s_rgDefaultTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);

            // validate translation
            VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse(Coord, uiButton, sModifierKeystate, sScrollDelta),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.X, Coord.Y));

        }


        // Default Tracking, SGR Encoding
        pInput->SetSGRExtendedMode(true);
        fExpectedKeyHandled = true; // SGR Mode should be able to handle any arbitrary coords.
        for (int i = 0; i < s_iTestCoordsLength; i++)
        {
            COORD Coord = s_rgTestCoords[i];

            Log::Comment(NoThrowString().Format(L"fHandled, x, y = (%d, %d, %d)", fExpectedKeyHandled, Coord.X, Coord.Y));
            s_pwszInputExpected = BuildSGRTestOutput(s_rgSgrTestOutput[i], uiButton, sModifierKeystate, sScrollDelta);

            // validate translation
            VERIFY_ARE_EQUAL(fExpectedKeyHandled, pInput->HandleMouse(Coord, uiButton, sModifierKeystate, sScrollDelta),
                             NoThrowString().Format(L"(x,y)=(%d,%d)", Coord.X, Coord.Y));

        }
    }
};
