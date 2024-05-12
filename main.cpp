#include <windows.h>
#include <filesystem>
#include <vector>
#include <mmsystem.h>
#include <sstream>
#include <iostream>
#include <shlobj_core.h> // file dialog
#pragma comment(lib, "winmm.lib")

namespace fs = std::filesystem;

std::wstring getExecutableDirectory() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
    return std::wstring(buffer).substr(0, pos);
}

std::vector<std::wstring> getSongList() {
    std::vector<std::wstring> songs;
    try {
        std::wstring exeDirectory = getExecutableDirectory();
        std::wstring songsFolder = exeDirectory + L"\\Songs";
        for (const auto& entry : fs::directory_iterator(songsFolder)) {
            if (entry.path().extension() == L".mp3" || entry.path().extension() == L".wav") {
                songs.push_back(entry.path().filename().wstring());
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error accessing directory: " << e.what() << std::endl;
    }
    return songs;
}

void playSong(const std::wstring& songName, bool resume = false) {
    static std::wstring lastPlayedSong;
    static bool isPaused = false;
    std::wstring exeDirectory = getExecutableDirectory();
    std::wstring songPath = exeDirectory + L"\\Songs\\" + songName;
    if (isPaused && lastPlayedSong == songPath && !resume) {
        mciSendStringW(L"resume mp3", NULL, 0, NULL);
        isPaused = false;
    }
    else {
        mciSendStringW(L"close mp3", NULL, 0, NULL); // close currently running title
        std::wstringstream command;
        command << L"open \"" << songPath << L"\" type mpegvideo alias mp3";
        mciSendStringW(command.str().c_str(), NULL, 0, NULL);
        mciSendStringW(L"play mp3", NULL, 0, NULL);
        lastPlayedSong = songPath;
        isPaused = false;
    }
}

void pauseSong() {
    mciSendStringW(L"pause mp3", NULL, 0, NULL);
}

void resumeSong() {
    mciSendStringW(L"resume mp3", NULL, 0, NULL);
}

void setVolume(int volume) {
    std::wstringstream command;
    command << L"setaudio mp3 volume to " << volume;
    mciSendStringW(command.str().c_str(), NULL, 0, NULL);
}

void saveSong(const std::wstring& songPath) {
    // Get executable directory
    std::wstring exeDirectory = getExecutableDirectory();
    std::wstring songsFolder = exeDirectory + L"\\Songs";
    if (!fs::exists(songsFolder)) {
        fs::create_directory(songsFolder);
    }
    // Copy file to Songs folder
    std::wstring destPath = songsFolder + L"\\" + fs::path(songPath).filename().wstring();
    fs::copy_file(songPath, destPath, fs::copy_options::overwrite_existing);
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static HWND hListBox; 
    static HWND hVolumeSlider; // vol slider handle
    static HWND hPauseResumeButton; // Pause/res button handle
    switch (msg)
    {
    case WM_CREATE:
        hListBox = CreateWindowW(L"LISTBOX", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_NOTIFY | LBS_WANTKEYBOARDINPUT | WS_VSCROLL, 10, 50, 360, 150, hwnd, NULL, NULL, NULL);
        if (hListBox == NULL) {
            return -1;
        }
        {
            std::vector<std::wstring> songList = getSongList();
            for (const auto& song : songList) {
                SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)song.c_str());
            }
        }
        // Create volume label
        CreateWindowW(L"STATIC", L"Volume", WS_VISIBLE | WS_CHILD, 10, 210, 50, 20, hwnd, NULL, NULL, NULL);
        // create volume slider
        hVolumeSlider = CreateWindowW(TRACKBAR_CLASS, NULL, WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS, 70, 210, 300, 30, hwnd, NULL, NULL, NULL);
        if (hVolumeSlider == NULL) {
            return -1;
        }
        SendMessage(hVolumeSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 400)); // Set volume range (0-200)
        SendMessage(hVolumeSlider, TBM_SETPOS, TRUE, 400);
        setVolume(200); 
        CreateWindowW(L"BUTTON", L"Play", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 10, 100, 30, hwnd, (HMENU)1, NULL, NULL);
        hPauseResumeButton = CreateWindowW(L"BUTTON", L"Pause", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 120, 10, 100, 30, hwnd, (HMENU)2, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Open File", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 230, 10, 100, 30, hwnd, (HMENU)3, NULL, NULL);
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            int selectedIndex = SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
            if (selectedIndex != LB_ERR) {
                wchar_t songName[MAX_PATH];
                SendMessageW(hListBox, LB_GETTEXT, selectedIndex, (LPARAM)songName);
                playSong(songName);
            }
        }
        else if (LOWORD(wParam) == 2) {
            static bool isPaused = false;
            if (!isPaused) {
                pauseSong();
                SetWindowTextW(hPauseResumeButton, L"Resume");
                isPaused = true;
            }
            else {
                resumeSong();
                SetWindowTextW(hPauseResumeButton, L"Pause");
                isPaused = false;
            }
        }
        else if (LOWORD(wParam) == 3) { 
            OPENFILENAME ofn;
            wchar_t szFile[MAX_PATH];
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            ofn.lpstrFile[0] = '\0';
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = L"All\0*.*\0MP3\0*.MP3\0WAV\0*.WAV\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrInitialDir = NULL;
            ofn.lpstrFileTitle = NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileName(&ofn) == TRUE) {
                playSong(szFile);
                saveSong(szFile);
            }
        }
        break;
    case WM_HSCROLL: // Volume slider changed
        if ((HWND)lParam == hVolumeSlider) {
            int volume = SendMessage(hVolumeSlider, TBM_GETPOS, 0, 0);
            setVolume(volume);
        }
        break;
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"Sample Window Class";

    WNDCLASS wc = { };

    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassW(&wc))
    {
        return 0;
    }

    HWND hwnd = CreateWindowW(CLASS_NAME, L"Music Player by Jopro1990", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, hInst, NULL);

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
