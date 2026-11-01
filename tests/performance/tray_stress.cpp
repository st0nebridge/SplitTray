// Module: tray_stress; purpose: bound refresh work under sustained notification bursts.
// Dependencies: literal production source and real hosted-XAML regression fixture.
#define main ExistingXamlTestsMain
#include "../xaml/xaml_tests.cpp"
#undef main

int main(int argc, char**) {
    CreateThread(nullptr,0,[](void*) -> DWORD { Sleep(30000); ExitProcess(124); },nullptr,0,nullptr);
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    if (!OpenIsland()) return 1;
    Settings s = EmbeddingSettings(); s.maxVisibleIcons = 1;
    ThreeIcons t = MakeThreeIcons(s);
    if (!t.tray) return 2;
    HICON red=MakeIcon(220,10,10), blue=MakeIcon(10,10,220);
    Feed(MakePayload(NIM_MODIFY,t.owner,2,NIF_ICON,nullptr,red));
    Feed(MakePayload(NIM_MODIFY,t.owner,3,NIF_ICON,nullptr,red));
    for (UINT id=4;id<=24;++id) AddIcon(t.owner,id,L"steady hidden icon",red);
    X::RefreshTray(*t.tray);
    X::ChevronReleased(CellsOf(t.tray)[0],kLeftDisplay,false);
    Pump();
    HWND shell=MakeOwner(); g_shellTrayWnd.store(shell);
    std::vector<double> times;
    LARGE_INTEGER frequency; QueryPerformanceFrequency(&frequency);
    for (int frame=0;frame<20;++frame) X::RefreshTray(*t.tray);
    const DWORD gdiBefore=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    const DWORD userBefore=GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
    DWORD handlesBefore=0; GetProcessHandleCount(GetCurrentProcess(),&handlesBefore);
    auto cpuTime=[] { FILETIME created,exited,kernel,user;
        GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user);
        ULARGE_INTEGER k,u; k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;
        u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;return k.QuadPart+u.QuadPart; };
    const auto cpuBefore=cpuTime();
    const ULONGLONG began=GetTickCount64();
    int queued=0;
    for (int frame=0;frame<240;++frame) {
        // Twenty arrivals before yielding must remain one queued wake.
        for (int event=0;event<20;++event) {
            Feed(MakePayload(NIM_MODIFY,t.owner,1,NIF_ICON|NIF_TIP,L"active icon",frame%2 ? red : blue));
            X::RequestEmbeddedRefresh();
        }
        int messages=0;
        while (TakePosted(shell,GetXamlRefreshMessage())) ++messages;
        if (messages!=1) return 3;
        queued+=messages;
        LARGE_INTEGER start,end; QueryPerformanceCounter(&start);
        ShellTrayWndSubclassProc(shell,GetXamlRefreshMessage(),0,0,0);
        QueryPerformanceCounter(&end);
        times.push_back(1000.0*(end.QuadPart-start.QuadPart)/frequency.QuadPart);
        Sleep(8);
    }
    const auto elapsed=GetTickCount64()-began;
    DWORD handlesAfter=0; GetProcessHandleCount(GetCurrentProcess(),&handlesAfter);
    const int gdiGrowth=static_cast<int>(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS))-gdiBefore;
    const int userGrowth=static_cast<int>(GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS))-userBefore;
    std::sort(times.begin(),times.end());
    const double p95=times[static_cast<size_t>(times.size()*0.95)], maximum=times.back();
    printf("{\"notifications\":4800,\"refreshes\":%d,\"elapsed_ms\":%llu,\"cpu_ms\":%.3f,\"p95_ms\":%.3f,\"max_ms\":%.3f,\"gdi_growth\":%d,\"user_growth\":%d,\"handle_growth\":%ld}\n",
        queued,elapsed,(cpuTime()-cpuBefore)/10000.0,p95,maximum,gdiGrowth,userGrowth,static_cast<long>(handlesAfter-handlesBefore));
    DestroyIcon(red);DestroyIcon(blue);DestroyWindow(shell);DestroyWindow(t.owner);
    ResetEverything(EmbeddingSettings());CloseIsland();
    // A single preallocated handle/picture may appear after the warm-up.
    return argc>1 || (p95<=16.7 && maximum<=250 && gdiGrowth<=16 && userGrowth<=16 && handlesAfter<=handlesBefore+16) ? 0:4;
}
