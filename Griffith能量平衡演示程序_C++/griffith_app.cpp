// C++17 / Win32 / GDI+. No Python or third-party GUI runtime is required.
#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#define NOMINMAX
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <richedit.h>
#include <shellapi.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "griffith_core.h"

using namespace Gdiplus;
using namespace griffith;
using std::wstring;
namespace fs = std::filesystem;

namespace theme {
const Color bg(255,13,20,29), panel(255,22,31,43), panel2(255,29,42,59);
const Color header(255,27,41,59), line(255,42,58,80), text(255,234,242,252);
const Color dim(255,148,168,193), blue(255,119,181,255), teal(255,50,214,180);
const Color gold(255,255,208,106), red(255,255,120,136), orange(255,255,155,88);
}
COLORREF rgb(Color c) { return RGB(c.GetR(), c.GetG(), c.GetB()); }
wstring fmt(double value, int digits = 4) {
    if (std::isinf(value)) return L"∞";
    if (!std::isfinite(value)) return L"超出范围";
    if (value == 0) return L"0";
    std::wostringstream out;
    if (std::abs(value) < .001 || std::abs(value) >= 100000)
        out << std::scientific << std::setprecision(std::max(2, digits - 1));
    else out << std::setprecision(digits);
    out << value;
    return out.str();
}
wstring fixed(double value, int digits) {
    std::wostringstream out; out << std::fixed << std::setprecision(digits) << value; return out.str();
}
wstring widen(const char* value) {
    int size = MultiByteToWideChar(CP_UTF8, 0, value, -1, nullptr, 0);
    wstring output(static_cast<std::size_t>(std::max(1,size)), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value, -1, output.data(), size);
    if (!output.empty()) output.pop_back();
    return output;
}
void fill(Graphics& g, const RectF& rect, Color color) { SolidBrush brush(color); g.FillRectangle(&brush, rect); }
void line(Graphics& g, PointF a, PointF b, Color color, float width = 1, bool dashed = false) {
    Pen pen(color, width); if (dashed) pen.SetDashStyle(DashStyleDash); g.DrawLine(&pen, a, b);
}
void label(Graphics& g, const wstring& text, float x, float y, Color color = theme::text,
           float size = 10, bool bold = false, float width = 2000,
           StringAlignment align = StringAlignmentNear) {
    Font font(L"Microsoft YaHei UI", size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    SolidBrush brush(color); StringFormat format;
    format.SetAlignment(align); format.SetTrimming(StringTrimmingEllipsisCharacter);
    format.SetFormatFlags(StringFormatFlagsNoWrap);
    g.DrawString(text.c_str(), -1, &font, RectF(x, y, width, size*1.65f), &format, &brush);
}
void wrapped(Graphics& g, const wstring& text, const RectF& rect, Color color = theme::dim, float size = 10) {
    Font font(L"Microsoft YaHei UI", size, FontStyleRegular, UnitPixel);
    SolidBrush brush(color); StringFormat format; format.SetTrimming(StringTrimmingEllipsisWord);
    g.DrawString(text.c_str(), -1, &font, rect, &format, &brush);
}
float textWidth(Graphics& g, const wstring& text, float size = 10) {
    Font font(L"Microsoft YaHei UI", size, FontStyleRegular, UnitPixel); RectF r;
    g.MeasureString(text.c_str(), -1, &font, PointF(0,0), &r); return r.Width;
}
class Surface {
    HWND hwnd_; PAINTSTRUCT ps_{}; HDC dc_, memory_; HBITMAP bitmap_, oldBitmap_; Graphics* graphics_; bool external_;
public:
    Surface(HWND hwnd, float scale, HDC supplied=nullptr) : hwnd_(hwnd), external_(supplied!=nullptr) {
        dc_ = supplied?supplied:BeginPaint(hwnd_, &ps_); RECT r{}; GetClientRect(hwnd_, &r);
        memory_ = CreateCompatibleDC(dc_);
        bitmap_ = CreateCompatibleBitmap(dc_, std::max(1L,r.right), std::max(1L,r.bottom));
        oldBitmap_ = static_cast<HBITMAP>(SelectObject(memory_, bitmap_));
        graphics_ = new Graphics(memory_); graphics_->SetSmoothingMode(SmoothingModeAntiAlias);
        graphics_->SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        graphics_->ScaleTransform(scale, scale);
    }
    Graphics& graphics() { return *graphics_; }
    ~Surface() {
        delete graphics_; RECT r{}; GetClientRect(hwnd_, &r);
        BitBlt(dc_, 0, 0, r.right, r.bottom, memory_, 0, 0, SRCCOPY);
        SelectObject(memory_, oldBitmap_); DeleteObject(bitmap_); DeleteDC(memory_); if(!external_)EndPaint(hwnd_, &ps_);
    }
};

struct Sample { double x, y; };
struct Series { wstring title; Color color; std::vector<Sample> points; };
struct Marker { double x; wstring title; Color color; int row = 0; std::vector<double> values; };
struct Chart {
    RectF area, bounds;
    wstring title, xlabel, ylabel, note;
    std::vector<Series> series; std::vector<Marker> markers;
    double xmin=0, xmax=1, ymin=0, ymax=1;
    bool fixedY = false;
};
struct V3 { double x,y,z; };
struct Projected { PointF point; double depth; };
struct App;
struct SliderData { App* app = nullptr; int index = 0; };

enum Command {
    Tab0=100, Tab1, Tab2, Front=110, Top, Iso, ResetView, Help,
    Edit0=200, Slider0=220, Stress=240, Strain, Update, Defaults, Animate, Export,
    Example0=260, Example1, Example2, Example3
};
const std::array<wstring,6> fieldNames = {L"弹性模量 E",L"泊松比 ν",L"单位表面能 γ",L"裂纹半长 a",L"远场拉应力 σ",L"板厚 B"};
const std::array<wstring,6> fieldUnits = {L"GPa",L"",L"J/m²",L"mm",L"MPa",L"mm"};
const std::array<double,6> minima = {1,0,.05,.01,0,.1}, maxima = {250,.49,10,3,30,20};
const std::array<int,6> fieldTop = {108,168,228,315,375,435};

LRESULT CALLBACK MainProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK LeftProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK RightProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK CanvasProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK SliderProc(HWND, UINT, WPARAM, LPARAM);

struct App {
    HINSTANCE instance; HWND window=nullptr, left=nullptr, right=nullptr, canvas=nullptr, results=nullptr;
    HFONT font=nullptr, mono=nullptr; HBRUSH editBrush=nullptr;
    std::array<HWND,6> edits{}, sliders{};
    std::array<SliderData,6> sliderData{};
    std::vector<HWND> leftButtons, rootButtons;
    Parameters p{}; Results q=calculate(p);
    float scale=1, width=1440, height=920, canvasW=800, canvasH=780;
    const float leftW=264, rightW=344, bodyY=120, footerH=29;
    int tab=0, leftOffset=0, scanIndex=0;
    bool setting=false, scanning=false, hover=false;
    int dragKind=0; PointF mouse{}, dragPoint{};
    double yaw=-26, tilt=38, zoom=1, panX=0, panY=0, halfspan=1.4;
    double scanLimit=1, scanEnergyLimit=1, scanSigmaYmax=1;
    std::vector<Chart> charts;
    wstring status, error;
    bool smokeMode=false;

    explicit App(HINSTANCE h):instance(h) {
        HDC dc=GetDC(nullptr); scale=GetDeviceCaps(dc, LOGPIXELSX)/96.0f; ReleaseDC(nullptr,dc);
        font=CreateFontW(-px(13),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
        mono=CreateFontW(-px(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Consolas");
        editBrush=CreateSolidBrush(rgb(theme::bg));
    }
    ~App() { DeleteObject(font); DeleteObject(mono); DeleteObject(editBrush); }
    int px(double logical) const { return static_cast<int>(std::lround(logical*scale)); }
    double* field(Parameters& parameters, int index) {
        double* values[] = {&parameters.E_gpa,&parameters.nu,&parameters.gamma,&parameters.a_mm,&parameters.sigma_mpa,&parameters.thickness_mm};
        return values[index];
    }
    void createControls(); void layout(); void layoutLeft(); void updateScroll();
    HWND addButton(HWND parent, int id, const wchar_t* title);
    void move(HWND control,float x,float y,float w,float h);
    void sync(); bool update(bool showError=false); void refresh(); void renderResults();
    void command(int id, int notification=0); void selectTab(int index); void example(int index);
    void preset(int id); void stopScan(); bool startScan(); void scanStep();
    double maxA() const; double energyMax() const; void buildCharts();
    std::vector<Marker> markers(std::vector<double> current={},std::vector<double> critical={}) const;
    void paintRoot(HDC supplied=nullptr); void paintLeft(HDC supplied=nullptr); void paintRight(HDC supplied=nullptr); void paintCanvas(HDC supplied=nullptr);
    void drawButton(const DRAWITEMSTRUCT& item); void drawChart(Graphics& g,Chart& chart);
    void drawModel(Graphics& g,const RectF& area); Projected project(V3 point,const RectF& area) const;
    void poly3(Graphics& g,const std::vector<V3>& vertices,const RectF& area,Color color,Color outline);
    void line3(Graphics& g,V3 a,V3 b,const RectF& area,Color color,float thickness=1,bool arrow=false,bool both=false);
    void label3(Graphics& g,V3 v,const RectF& area,const wstring& text,Color color);
    void scrollLeft(int code,int position=0); void sliderChanged(int index,double value);
    void mouseDown(PointF point,int kind); void mouseMove(PointF point); void mouseUp(); void wheel(int delta,PointF point);
    void pick(double a); bool writeCsv(const fs::path& path); void exportCsv(); void showHelp();
    bool capture(const fs::path& path); int smokeTest(const fs::path& directory);
};

HWND App::addButton(HWND parent,int id,const wchar_t* title) {
    HWND button=CreateWindowW(L"BUTTON",title,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
                             0,0,1,1,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
    SendMessageW(button,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
    if(parent==left) leftButtons.push_back(button); else rootButtons.push_back(button);
    return button;
}
void App::createControls() {
    left=CreateWindowExW(WS_EX_CONTROLPARENT,L"GriffithLeft",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL,0,0,1,1,window,nullptr,instance,this);
    right=CreateWindowExW(WS_EX_CONTROLPARENT,L"GriffithRight",L"",WS_CHILD|WS_VISIBLE,0,0,1,1,window,nullptr,instance,this);
    canvas=CreateWindowExW(0,L"GriffithCanvas",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,1,1,window,nullptr,instance,this);
    for(int i=0;i<6;++i) {
        edits[i]=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_RIGHT|ES_AUTOHSCROLL,
                               0,0,1,1,left,reinterpret_cast<HMENU>(static_cast<INT_PTR>(Edit0+i)),instance,nullptr);
        SendMessageW(edits[i],WM_SETFONT,reinterpret_cast<WPARAM>(mono),TRUE);
        SendMessageW(edits[i],EM_SETLIMITTEXT,120,0);
        sliderData[i]={this,i};
        sliders[i]=CreateWindowExW(0,L"GriffithSlider",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,1,1,left,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(Slider0+i)),instance,&sliderData[i]);
    }
    addButton(left,Stress,L"平面应力 · 薄板近似"); addButton(left,Strain,L"平面应变 · 厚板约束");
    addButton(left,Update,L"计算 / 更新"); addButton(left,Defaults,L"恢复默认");
    addButton(left,Animate,L"▶ 裂纹扫描"); addButton(left,Export,L"导出数据");
    addButton(left,Example0,L"例1 亚临界"); addButton(left,Example1,L"例2 临界平衡");
    addButton(left,Example2,L"例3 超临界"); addButton(left,Example3,L"例4 平面应变");
    addButton(window,Front,L"正视"); addButton(window,Top,L"俯视"); addButton(window,Iso,L"等轴测");
    addButton(window,ResetView,L"复位视角"); addButton(window,Help,L"公式与说明");
    addButton(window,Tab0,L"① 裂纹模型 · 临界判定"); addButton(window,Tab1,L"② 能量平衡 · 能量曲线");
    addButton(window,Tab2,L"③ 参数扫描 · σc–a 曲线");
    results=CreateWindowExW(0,MSFTEDIT_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,
                           0,0,1,1,right,nullptr,instance,nullptr);
    if(!results) throw std::runtime_error("Windows RichEdit control could not be created.");
    SendMessageW(results,EM_SETBKGNDCOLOR,0,rgb(theme::bg));
    SendMessageW(results,WM_SETFONT,reinterpret_cast<WPARAM>(font),FALSE);
    SendMessageW(results,EM_SETTARGETDEVICE,0,0);
    SendMessageW(results,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(px(12),px(12)));
    sync(); layout(); refresh();
}
void App::move(HWND control,float x,float y,float w,float h) { MoveWindow(control,px(x),px(y),px(w),px(h),TRUE); }
void App::layout() {
    RECT r{}; GetClientRect(window,&r); width=r.right/scale; height=r.bottom/scale;
    float bodyH=std::max(120.0f,height-bodyY-footerH);
    canvasW=std::max(100.0f,width-leftW-rightW-18); canvasH=bodyH;
    move(left,8,bodyY,leftW,bodyH); move(canvas,9+leftW,bodyY,canvasW,bodyH);
    move(right,10+leftW+canvasW,bodyY,rightW,bodyH);
    move(results,8,91,rightW-16,bodyH-99);
    float navX=std::max(590.0f,width-453);
    for(std::size_t i=0;i<rootButtons.size();++i) {
        int id=GetDlgCtrlID(rootButtons[i]);
        if(id>=Tab0 && id<=Tab2) {
            float widths[]={228,235,246}; float x=12;
            for(int k=0;k<id-Tab0;++k) x+=widths[k]+4;
            move(rootButtons[i],x,77,widths[id-Tab0],35);
        } else {
            int index=id-Front; float widths[]={60,60,75,94,116}; float x=navX;
            for(int k=0;k<index;++k) x+=widths[k]+3;
            move(rootButtons[i],x,18,widths[index],32);
        }
    }
    updateScroll(); layoutLeft(); buildCharts();
    InvalidateRect(window,nullptr,FALSE); InvalidateRect(canvas,nullptr,FALSE); InvalidateRect(right,nullptr,FALSE);
}
void App::updateScroll() {
    RECT r{}; GetClientRect(left,&r); int visible=std::max(1,static_cast<int>(r.bottom/scale));
    leftOffset=std::clamp(leftOffset,0,std::max(0,990-visible));
    SCROLLINFO si{sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS,0,989,static_cast<UINT>(visible),leftOffset,0};
    SetScrollInfo(left,SB_VERT,&si,TRUE);
}
void App::layoutLeft() {
    for(int i=0;i<6;++i) {
        move(edits[i],127,fieldTop[i]-leftOffset,76,28);
        move(sliders[i],12,fieldTop[i]+35-leftOffset,228,18);
    }
    for(HWND button:leftButtons) {
        int id=GetDlgCtrlID(button); float x=12,y=0,w=228,h=29;
        switch(id) {
            case Stress: y=528; break; case Strain:y=559;break;
            case Update:y=599;w=131;h=35;break; case Defaults:x=148;y=599;w=92;h=35;break;
            case Animate:y=640;w=131;h=35;break; case Export:x=148;y=640;w=92;h=35;break;
            default: {
                int index=id-Example0; x=12+(index%2)*117; y=723+(index/2)*40;w=111;h=35; break;
            }
        }
        move(button,x,y-leftOffset,w,h);
    }
    InvalidateRect(left,nullptr,FALSE);
}
void App::sync() {
    setting=true;
    for(int i=0;i<6;++i) {
        std::wostringstream value; value<<std::setprecision(12)<<*field(p,i);
        SetWindowTextW(edits[i],value.str().c_str()); InvalidateRect(sliders[i],nullptr,FALSE);
    }
    for(HWND b:leftButtons) InvalidateRect(b,nullptr,FALSE);
    setting=false;
}

bool App::update(bool showError) {
    KillTimer(window,1);
    error.clear();
    try {
        Parameters candidate=p;
        for(int i=0;i<6;++i) {
            wchar_t buffer[128]{}; GetWindowTextW(edits[i],buffer,128);
            wchar_t* end=nullptr; const double value=std::wcstod(buffer,&end);
            while(end && std::iswspace(*end)) ++end;
            if(end==buffer || !end || *end) throw std::runtime_error("Invalid number");
            if(!std::isfinite(value)) { error=fieldNames[i]+L" 必须为有限数值。"; throw std::invalid_argument("Invalid value"); }
            if((i==0||i==2||i==3||i==5)&&value<=0) { error=fieldNames[i]+L" 必须大于 0。"; throw std::invalid_argument("Nonpositive input"); }
            if(i==1&&(value<0||value>=.5)) { error=L"泊松比 ν 必须满足 0 ≤ ν < 0.5。"; throw std::invalid_argument("Poisson ratio"); }
            if(i==4&&value<0) { error=L"远场拉应力 σ 必须大于或等于 0。"; throw std::invalid_argument("Negative stress"); }
            *field(candidate,i)=value;
        }
        Results result=calculate(candidate);
        // Also validate the plotting range before accepting the candidate.
        const double maxa=std::max({candidate.a_mm*1.8,
            std::min(std::isfinite(result.a_c_mm)?result.a_c_mm*1.65:candidate.a_mm*2.5,candidate.a_mm*6),.2});
        curve_points(candidate,maxa,2);
        p=candidate; q=result; error.clear();
        if(!scanning && p.a_mm>halfspan*.85) halfspan=p.a_mm*1.4;
        sync(); refresh(); return true;
    } catch(const std::exception& exc) {
        if(error.empty()) error=std::string(exc.what())=="Invalid number" ? L"输入必须填写完整的数字。" : L"参数量级超出计算范围，请调整输入。";
        status=L"输入未生效："+error;
        InvalidateRect(window,nullptr,FALSE);
        if(showError && !smokeMode) MessageBoxW(window,error.c_str(),L"参数检查",MB_OK|MB_ICONWARNING);
        return false;
    }
}
void App::refresh() {
    buildCharts(); renderResults();
    if(!scanning) status=(p.state==State::plane_stress?L"平面应力":L"平面应变")+
        wstring(L"  |  a = ")+fmt(p.a_mm)+L" mm，2a = "+fmt(2*p.a_mm)+L" mm  |  σc = "+fmt(q.sigma_c_mpa)+
        L" MPa  |  G / 2γ = "+fixed(q.ratio,4)+L"  |  Enter 更新 · Esc 停止扫描";
    InvalidateRect(window,nullptr,FALSE); InvalidateRect(canvas,nullptr,FALSE); InvalidateRect(right,nullptr,FALSE);
}
void App::selectTab(int index) {
    tab=index; hover=false; buildCharts();
    for(HWND b:rootButtons) InvalidateRect(b,nullptr,FALSE);
    InvalidateRect(canvas,nullptr,FALSE);
}
void App::example(int index) {
    stopScan(); p=Parameters{}; halfspan=1.4;
    if(index==1) p.sigma_mpa=calculate(p).sigma_c_mpa;
    else if(index==2) p.sigma_mpa=12;
    else if(index==3) p.state=State::plane_strain;
    error.clear(); sync(); update();
}
void App::preset(int id) {
    selectTab(0); yaw=id==Front||id==Top?0:-26; tilt=id==Front?0:id==Top?76:38;
    panX=panY=0;zoom=1;InvalidateRect(canvas,nullptr,FALSE);
}
void App::command(int id,int notification) {
    if(id>=Edit0 && id<Edit0+6) {
        if(notification==EN_KILLFOCUS && !setting) { error.clear(); SetTimer(window,1,90,nullptr); }
        return;
    }
    error.clear();
    if(id>=Tab0 && id<=Tab2) selectTab(id-Tab0);
    else if(id>=Front && id<=ResetView) preset(id);
    else if(id>=Example0 && id<=Example3) example(id-Example0);
    else switch(id) {
        case Update:stopScan();update(true);break;
        case Defaults:example(0);break;
        case Stress:case Strain: {
            const State accepted=p.state;p.state=id==Stress?State::plane_stress:State::plane_strain;
            if(!update()) {p.state=accepted;for(HWND b:leftButtons)InvalidateRect(b,nullptr,FALSE);}
            break;
        }
        case Animate:if(scanning){stopScan();update();}else startScan();break;
        case Export:exportCsv();break;
        case Help:showHelp();break;
    }
}
void App::stopScan() {
    scanning=false; KillTimer(window,2);
    for(HWND b:leftButtons) if(GetDlgCtrlID(b)==Animate) { SetWindowTextW(b,L"▶ 裂纹扫描");InvalidateRect(b,nullptr,FALSE); }
}
bool App::startScan() {
    if(!update(true)) return false;
    scanLimit=maxA(); scanEnergyLimit=energyMax(); scanSigmaYmax=std::max({q.sigma_c_mpa*2.8,p.sigma_mpa*1.55,.01});
    halfspan=std::max(scanLimit*1.25,.2); scanIndex=0; scanning=true;
    for(HWND b:leftButtons) if(GetDlgCtrlID(b)==Animate) { SetWindowTextW(b,L"■ 停止扫描");InvalidateRect(b,nullptr,FALSE); }
    SetTimer(window,2,80,nullptr); return true;
}
void App::scanStep() {
    if(!scanning) return;
    if(++scanIndex>100) { stopScan();update();return; }
    p.a_mm=scanLimit*(.05+.95*scanIndex/100.0);sync();
    if(!update()) { stopScan();return; }
    status=L"教学扫描 "+std::to_wstring(scanIndex)+L"%  |  递增裂纹半长，展示参数关系；不代表真实时间扩展  |  Esc 停止";
    InvalidateRect(window,nullptr,FALSE);SetTimer(window,2,110,nullptr);
}
double App::maxA() const {
    if(scanning) return scanLimit;
    double candidate=std::isfinite(q.a_c_mm)?q.a_c_mm*1.65:p.a_mm*2.5;
    return std::max({p.a_mm*1.8,std::min(candidate,p.a_mm*6),.2});
}
double App::energyMax() const {
    if(scanning) return scanEnergyLimit;
    return std::isfinite(q.a_c_mm)?std::max(maxA(),std::min(q.a_c_mm*2.45,p.a_mm*8)):maxA();
}
std::vector<Marker> App::markers(std::vector<double> current,std::vector<double> critical) const {
    std::vector<Marker> result={{p.a_mm,L"当前 a="+fmt(p.a_mm),theme::blue,1,std::move(current)}};
    if(std::isfinite(q.a_c_mm)) result.push_back({q.a_c_mm,L"临界 ac="+fmt(q.a_c_mm),theme::gold,0,std::move(critical)});
    return result;
}
void App::buildCharts() {
    if(canvasW<100||canvasH<100) return;
    charts.clear();
    const double maxa=maxA(), emax=energyMax();
    if(tab==0) {
        Chart chart; chart.area=RectF(0,canvasH-252,canvasW,252);
        chart.title=L"扩展驱动力与表面能阻力";chart.xlabel=L"裂纹半长 a (mm)";chart.ylabel=L"能量 / 面积 (J/m²)";
        chart.note=L"点击曲线区域可选取裂纹半长。";
        Series g{L"G",theme::teal,{}};for(const auto& pt:curve_points(p,maxa,241)) g.points.push_back({pt.a_mm,pt.G_j_m2});
        chart.series={std::move(g),{L"R=2γ",theme::gold,{{0,q.R_j_m2},{maxa,q.R_j_m2}}}};
        chart.markers=markers({q.G_j_m2},{q.R_j_m2});charts.push_back(std::move(chart));
    } else if(tab==1) {
        auto points=curve_points(p,emax,301);
        Series u{L"释放 Urel",theme::teal,{}},s{L"表面 S",theme::gold,{}},delta{L"ΔΠ",theme::orange,{}};
        for(const auto& pt:points) { u.points.push_back({pt.a_mm,pt.U_release_j*1000});
            s.points.push_back({pt.a_mm,pt.S_surface_j*1000});delta.points.push_back({pt.a_mm,-pt.net_release_j*1000}); }
        double sc=std::isfinite(q.a_c_mm)?4*q.a_c_mm/1000*q.B_m*p.gamma*1000:0;
        Chart energy;energy.area=RectF(0,0,canvasW,canvasH/2);energy.title=L"有效释放能量与新表面能";
        energy.xlabel=L"裂纹半长 a (mm)";energy.ylabel=L"能量 (mJ)";
        energy.note=L"临界条件是斜率平衡；总能量曲线交点不是起裂点。";
        energy.series={std::move(u),std::move(s)};
        energy.markers=markers({q.U_release_j*1000,q.S_surface_j*1000},{sc/2,sc});
        if(std::isfinite(q.a_c_mm)) energy.markers.push_back({2*q.a_c_mm,L"总能量交点 2ac",theme::dim,2,{sc*2}});
        charts.push_back(std::move(energy));
        Chart net;net.area=RectF(0,canvasH/2,canvasW,canvasH/2);net.title=L"总能量变化 ΔΠ = S − Urel";
        net.xlabel=L"裂纹半长 a (mm)";net.ylabel=L"能量变化 (mJ)";
        net.note=L"固定远场应力：ac 为能垒顶点，dΔΠ/dA = 2γ − G。";
        net.series={std::move(delta)};net.markers=markers({-q.net_release_j*1000},{sc/2});
        if(std::isfinite(q.a_c_mm)) net.markers.push_back({2*q.a_c_mm,L"ΔΠ=0（2ac）",theme::dim,2,{0}});
        charts.push_back(std::move(net));
    } else {
        Chart scan;scan.area=RectF(0,0,canvasW,std::max(240.0f,canvasH-150));scan.title=L"裂纹尺寸与剩余强度 σc";
        scan.xlabel=L"裂纹半长 a (mm)";scan.ylabel=L"临界 / 外加应力 (MPa)";
        scan.note=L"σc ∝ a⁻¹ᐟ²；点击曲线选择 a，纵轴范围外的部分省略。";
        double mina=std::max(maxa/250,1e-6);
        Series strength{L"临界 σc",theme::teal,{}};
        for(int i=0;i<=300;++i) { double a=mina+(maxa-mina)*i/300;
            strength.points.push_back({a,std::sqrt(2*q.E_prime_pa*p.gamma/(pi*(a/1000)))/1e6}); }
        scan.series={std::move(strength),{L"外加 σ",theme::blue,{{mina,p.sigma_mpa},{maxa,p.sigma_mpa}}}};
        scan.markers=markers({q.sigma_c_mpa},{p.sigma_mpa});scan.fixedY=true;scan.ymin=0;
        scan.ymax=scanning?scanSigmaYmax:std::max({q.sigma_c_mpa*2.8,p.sigma_mpa*1.55,.01});charts.push_back(std::move(scan));
    }
}
void App::renderResults() {
    if(!results) return;
    POINT scroll{};SendMessageW(results,EM_GETSCROLLPOS,0,reinterpret_cast<LPARAM>(&scroll));
    SendMessageW(results,WM_SETREDRAW,FALSE,0);SetWindowTextW(results,L"");
    auto add=[&](const wstring& text,Color color=theme::text,bool bold=false) {
        SendMessageW(results,EM_SETSEL,-1,-1);
        CHARFORMAT2W format{};format.cbSize=sizeof(format);format.dwMask=CFM_COLOR|CFM_SIZE|CFM_FACE|CFM_BOLD;
        format.crTextColor=rgb(color);format.yHeight=200;format.dwEffects=bold?CFE_BOLD:0;
        std::wcscpy(format.szFaceName,L"Microsoft YaHei UI");
        SendMessageW(results,EM_SETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&format));
        wstring row=text+L"\r\n";SendMessageW(results,EM_REPLACESEL,FALSE,reinterpret_cast<LPARAM>(row.c_str()));
    };
    auto heading=[&](const wstring& text) {add(L"");add(text,theme::blue,true);};
    auto formula=[&](const wstring& text) {add(text,theme::gold);};
    auto value=[&](const wstring& text) {add(text,theme::teal);};
    add(L"【一】输入与单位转换",theme::blue,true);
    add(L"E = "+fmt(p.E_gpa)+L" GPa = "+fmt(p.E_gpa*1e9)+L" Pa");
    add(L"ν = "+fmt(p.nu)+L"；γ = "+fmt(p.gamma)+L" J/m²");
    add(L"a = "+fmt(p.a_mm)+L" mm = "+fmt(q.a_m)+L" m");
    add(L"2a = "+fmt(2*p.a_mm)+L" mm（裂纹全长）");
    add(L"σ = "+fmt(p.sigma_mpa)+L" MPa = "+fmt(q.sigma_pa)+L" Pa");
    add(L"B = "+fmt(p.thickness_mm)+L" mm = "+fmt(q.B_m)+L" m");
    heading(L"【二】等效弹性模量 E′");
    formula(p.state==State::plane_stress?L"平面应力：E′ = E":L"平面应变：E′ = E / (1 − ν²)");
    if(p.state==State::plane_strain) add(L"= "+fmt(p.E_gpa)+L" / (1 − "+fmt(p.nu)+L"²) GPa");
    value(L"E′ = "+fmt(q.E_prime_pa/1e9,6)+L" GPa");
    heading(L"【三】有效释放能量与新表面能");
    formula(L"A = 2aB（裂纹投影面积）");add(L"A = "+fmt(q.A_m2)+L" m²；双表面面积 = 2A");
    formula(L"Urel = πa²σ²B / E′");
    add(L"= π × ("+fmt(q.a_m)+L")² × ("+fmt(q.sigma_pa)+L")²");
    add(L"  × "+fmt(q.B_m)+L" / "+fmt(q.E_prime_pa));value(L"Urel = "+fmt(q.U_release_j*1000,6)+L" mJ");
    formula(L"S = 2γA = 4aBγ");add(L"= 4 × "+fmt(q.a_m)+L" × "+fmt(q.B_m)+L" × "+fmt(p.gamma));
    value(L"S = "+fmt(q.S_surface_j*1000,6)+L" mJ");add(L"ΔΠ = S − Urel = "+fmt(-q.net_release_j*1000,6)+L" mJ");
    add(L"恒载时外力继续做功，应变能可增加；Urel 表示外力功扣除应变能变化后的有效释放能量。",theme::dim);
    heading(L"【四】Griffith 能量平衡判据");formula(L"G = dUrel/dA = πaσ²/E′");
    add(L"= π × "+fmt(q.a_m)+L" × ("+fmt(q.sigma_pa)+L")²");add(L"  / "+fmt(q.E_prime_pa));
    value(L"G = "+fmt(q.G_j_m2,6)+L" J/m²");formula(L"R = dS/dA = 2γ");value(L"R = "+fmt(q.R_j_m2,6)+L" J/m²");
    value(L"G / R = "+fixed(q.ratio,6));add(L"dΔΠ/dA = R − G = "+fmt(q.R_j_m2-q.G_j_m2,6)+L" J/m²");
    add(q.classification==Classification::subcritical?L"能量变化率尚不足以克服表面能阻力。":
        q.classification==Classification::critical?L"能量变化率平衡，达到扩展临界状态。":L"释放率超过表面能阻力，具有扩展驱动力。",theme::orange);
    heading(L"【五】临界应力与临界裂纹半长");formula(L"σc = √(2E′γ / πa)");
    add(L"= √[2 × "+fmt(q.E_prime_pa)+L" × "+fmt(p.gamma));add(L"     / (π × "+fmt(q.a_m)+L")]");
    value(L"σc = "+fmt(q.sigma_c_mpa,6)+L" MPa");formula(L"ac = 2E′γ / (πσ²)");
    if(p.sigma_mpa==0) add(L"σ = 0：ac = ∞，不存在有限临界半长。",theme::dim);
    else { value(L"ac = "+fmt(q.a_c_mm,6)+L" mm（临界半长）");add(L"2ac = "+fmt(2*q.a_c_mm,6)+L" mm（临界全长）"); }
    heading(L"【六】与应力强度因子的关系");formula(L"KI = σ√(πa)，G = KI²/E′");
    value(L"KI = "+fmt(q.K_mpa_sqrt_m,6)+L" MPa·√m");heading(L"【七】理解临界条件");
    add(L"临界时比较的是能量变化率。",theme::dim);formula(L"在 a = ac 时：Urel = S/2。");
    add(L"Urel = S 的总能量交点在 a = 2ac。",theme::dim);
    add(L"G = R 判断扩展临界；持续失稳还与加载约束有关。",theme::dim);
    heading(L"【八】适用范围");add(L"无限大板、中心贯穿裂纹、均匀远场拉伸。",theme::dim);
    add(L"材料整体线弹性，经典模型仅计表面能。",theme::dim);
    add(L"理想恒应力曲线不等同于有限试样固定端位移全过程。",theme::dim);
    add(L"不包含塑性功、裂纹扩展速率或有限宽修正。",theme::dim);
    SendMessageW(results,EM_SETSEL,0,0);SendMessageW(results,EM_SETSCROLLPOS,0,reinterpret_cast<LPARAM>(&scroll));
    SendMessageW(results,WM_SETREDRAW,TRUE,0);InvalidateRect(results,nullptr,TRUE);
}

void App::paintRoot(HDC supplied) {
    Surface surface(window,scale,supplied);auto& g=surface.graphics();
    fill(g,RectF(0,0,width,height),theme::bg);fill(g,RectF(0,0,width,66),theme::header);
    label(g,L"Griffith 能量平衡原理 · 计算与可视化",17,12,theme::text,21,true,width-485);
    label(g,L"G = πaσ² / E′     R = 2γ     σc = √(2E′γ / πa)     |     C++17 桌面版",18,45,theme::gold,11);
    label(g,L"经典 Griffith · 理想脆性材料",width-196,88,theme::dim,10,false,184,StringAlignmentFar);
    fill(g,RectF(0,height-footerH,width,footerH),theme::header);
    label(g,status,12,height-footerH+6,error.empty()?theme::dim:theme::red,10,false,width-24);
}
void App::paintLeft(HDC supplied) {
    Surface surface(left,scale,supplied);auto& g=surface.graphics();RECT r{};GetClientRect(left,&r);
    fill(g,RectF(0,0,r.right/scale,r.bottom/scale),theme::panel);
    g.TranslateTransform(0,-static_cast<float>(leftOffset));
    label(g,L"输入参数",14,15,theme::text,17,true);
    label(g,L"中心贯穿裂纹，输入 a 为半长。",14,49,theme::dim,11);
    label(g,L"材料参数",14,81,theme::blue,13,true);
    label(g,L"裂纹与载荷",14,286,theme::blue,13,true);
    for(int i=0;i<6;++i) {
        label(g,fieldNames[i],14,fieldTop[i]+6,theme::dim,11,false,109);
        label(g,fieldUnits[i],208,fieldTop[i]+6,theme::dim,10,false,38);
    }
    label(g,L"二维约束状态",14,497,theme::blue,13,true);
    label(g,L"教学示例（点击载入）",14,692,theme::blue,13,true);
    wrapped(g,L"示例为教学参数。滑块自动更新。\n直接输入后按 Enter。\n板厚不会自动决定二维约束状态。",RectF(14,813,226,62),theme::dim,11);
    label(g,L"适用模型",14,890,theme::blue,13,true);
    wrapped(g,L"无限大板 · 中心贯穿裂纹 · Ⅰ型拉伸\n线弹性 · 理想脆性 · 仅计表面能\n不含塑性功与有限宽修正。",RectF(14,920,226,66),theme::dim,11);
}
void App::paintRight(HDC supplied) {
    Surface surface(right,scale,supplied);auto& g=surface.graphics();RECT r{};GetClientRect(right,&r);
    fill(g,RectF(0,0,r.right/scale,r.bottom/scale),theme::panel);
    label(g,L"计算结果与公式对照",14,15,theme::text,16,true,rightW-28);
    Color color=q.classification==Classification::subcritical?theme::teal:q.classification==Classification::critical?theme::gold:theme::red;
    Color background=q.classification==Classification::subcritical?Color(255,21,55,47):
        q.classification==Classification::critical?Color(255,59,51,32):Color(255,60,37,48);
    fill(g,RectF(10,48,rightW-20,36),background);
    label(g,q.classification==Classification::subcritical?L"亚临界  G < 2γ":
        q.classification==Classification::critical?L"临界平衡  G = 2γ":L"超临界  G > 2γ",22,55,color,14,true);
}
void App::drawButton(const DRAWITEMSTRUCT& item) {
    if(item.CtlType!=ODT_BUTTON) return;
    Graphics g(item.hDC);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);g.ScaleTransform(scale,scale);
    RectF rect(item.rcItem.left/scale,item.rcItem.top/scale,(item.rcItem.right-item.rcItem.left)/scale,(item.rcItem.bottom-item.rcItem.top)/scale);
    int id=static_cast<int>(item.CtlID);bool selected=(id>=Tab0&&id<=Tab2&&id-Tab0==tab)||id==Update;
    bool radio=id==Stress||id==Strain;bool pressed=(item.itemState&ODS_SELECTED)!=0;
    Color background=selected?Color(255,43,76,114):theme::panel2;
    if(id>=Front && id<=Help) background=theme::header;
    if(radio) background=theme::panel;
    if(pressed) background=Color(255,53,93,137);
    fill(g,rect,background);
    wchar_t caption[150]{};GetWindowTextW(item.hwndItem,caption,150);
    if(radio) {
        Pen pen(theme::dim,1);g.DrawEllipse(&pen,rect.X+6,rect.Y+9,11.0f,11.0f);
        bool checked=(id==Stress&&p.state==State::plane_stress)||(id==Strain&&p.state==State::plane_strain);
        if(checked) {SolidBrush brush(theme::blue);g.FillEllipse(&brush,rect.X+9,rect.Y+12,5.0f,5.0f);}
        label(g,caption,rect.X+25,rect.Y+5,theme::text,12,false,rect.Width-29);
    } else {
        label(g,caption,rect.X+3,rect.Y+(rect.Height-18)/2,selected?theme::text:theme::dim,
              id>=Tab0&&id<=Tab2?12.0f:11.5f,false,rect.Width-6,StringAlignmentCenter);
    }
    if(item.itemState&ODS_FOCUS) {
        Pen pen(theme::blue,1);pen.SetDashStyle(DashStyleDot);g.DrawRectangle(&pen,rect.X+3,rect.Y+3,rect.Width-6,rect.Height-6);
    }
}
void App::drawChart(Graphics& g,Chart& chart) {
    const RectF area=chart.area;
    chart.bounds=RectF(area.X+72,area.Y+88,area.Width-90,area.Height-155);
    if(chart.bounds.Width<25||chart.bounds.Height<25) return;
    double xmin=std::numeric_limits<double>::infinity(),xmax=-xmin;
    double ymin=0,ymax=0;
    for(const auto& series:chart.series) for(auto v:series.points) if(std::isfinite(v.x)&&std::isfinite(v.y)) {
        xmin=std::min(xmin,v.x);xmax=std::max(xmax,v.x);ymin=std::min(ymin,v.y);ymax=std::max(ymax,v.y);
    }
    if(!std::isfinite(xmin)||!std::isfinite(xmax)) return;
    chart.xmin=xmin;chart.xmax=xmax>xmin?xmax:xmin+1;
    if(!chart.fixedY) { if(ymax<=ymin)ymax=ymin+1;double range=ymax-ymin;ymax+=range*.12;if(ymin<0)ymin-=range*.05;chart.ymin=ymin;chart.ymax=ymax; }
    auto xy=[&](double x,double y) {
        return PointF(chart.bounds.X+static_cast<float>((x-chart.xmin)/(chart.xmax-chart.xmin)*chart.bounds.Width),
            chart.bounds.GetBottom()-static_cast<float>((y-chart.ymin)/(chart.ymax-chart.ymin)*chart.bounds.Height));
    };
    label(g,chart.title,area.X+20,area.Y+14,theme::text,16,true,area.Width-40);
    float legendX=area.X+22;
    for(const auto& series:chart.series) {
        line(g,PointF(legendX,area.Y+50),PointF(legendX+20,area.Y+50),series.color,2.5f);
        label(g,series.title,legendX+26,area.Y+42,series.color,11);legendX+=textWidth(g,series.title,11)+50;
    }
    label(g,chart.ylabel,chart.bounds.X,chart.bounds.Y-23,theme::dim,10,false,chart.bounds.Width);
    for(int i=0;i<6;++i) {
        double x=chart.xmin+(chart.xmax-chart.xmin)*i/5;
        double y=chart.ymin+(chart.ymax-chart.ymin)*i/5;
        PointF px=xy(x,chart.ymin),py=xy(chart.xmin,y);
        line(g,PointF(px.X,chart.bounds.Y),PointF(px.X,chart.bounds.GetBottom()),Color(255,29,43,60));
        line(g,PointF(chart.bounds.X,py.Y),PointF(chart.bounds.GetRight(),py.Y),Color(255,29,43,60));
        label(g,fmt(x,3),px.X-40,chart.bounds.GetBottom()+8,theme::dim,10,false,80,StringAlignmentCenter);
        label(g,fmt(y,3),area.X+2,py.Y-7,theme::dim,10,false,62,StringAlignmentFar);
    }
    line(g,PointF(chart.bounds.X,chart.bounds.Y),PointF(chart.bounds.X,chart.bounds.GetBottom()),Color(255,83,101,128));
    line(g,PointF(chart.bounds.X,chart.bounds.GetBottom()),PointF(chart.bounds.GetRight(),chart.bounds.GetBottom()),Color(255,83,101,128));
    label(g,chart.xlabel,chart.bounds.X,chart.bounds.GetBottom()+30,theme::dim,11,false,chart.bounds.Width,StringAlignmentCenter);
    if(chart.ymin<0&&chart.ymax>0) line(g,xy(chart.xmin,0),xy(chart.xmax,0),Color(255,72,89,113),1,true);
    GraphicsState saved=g.Save();g.SetClip(chart.bounds);
    for(const auto& series:chart.series) {
        for(std::size_t i=1;i<series.points.size();++i) {
            auto a=series.points[i-1],b=series.points[i];
            if(!std::isfinite(a.y)||!std::isfinite(b.y)) continue;
            // Clip in data coordinates before converting to floats, including steep sigma_c segments.
            if((a.y<chart.ymin&&b.y<chart.ymin)||(a.y>chart.ymax&&b.y>chart.ymax)) continue;
            auto cut=[&](Sample& end,const Sample& other) {
                double limit=std::clamp(end.y,chart.ymin,chart.ymax);
                if(limit!=end.y && other.y!=end.y) {end.x+=(other.x-end.x)*(limit-end.y)/(other.y-end.y);end.y=limit;}
            };
            cut(a,b);cut(b,a);line(g,xy(a.x,a.y),xy(b.x,b.y),series.color,2.4f);
        }
    }
    g.Restore(saved);
    for(const auto& mark:chart.markers) if(mark.x>=chart.xmin&&mark.x<=chart.xmax) {
        float x=xy(mark.x,chart.ymin).X;
        line(g,PointF(x,chart.bounds.Y),PointF(x,chart.bounds.GetBottom()),mark.color,1,true);
        bool toLeft=x>chart.bounds.X+chart.bounds.Width*.58;
        float available=toLeft?x-chart.bounds.X-12:chart.bounds.GetRight()-x-12;
        float textW=std::min(200.0f,std::max(30.0f,available));
        label(g,mark.title,toLeft?x-textW-6:x+6,chart.bounds.Y+5+mark.row*18,mark.color,10,false,textW,toLeft?StringAlignmentFar:StringAlignmentNear);
        for(double v:mark.values) if(std::isfinite(v)&&v>=chart.ymin&&v<=chart.ymax) {
            PointF point=xy(mark.x,v);SolidBrush brush(mark.color);g.FillEllipse(&brush,point.X-3.5f,point.Y-3.5f,7.0f,7.0f);
        }
    }
    label(g,chart.note,area.X+20,area.GetBottom()-23,theme::dim,10,false,area.Width-40);
    if(hover&&chart.bounds.Contains(mouse)) {
        const double x=chart.xmin+(mouse.X-chart.bounds.X)/chart.bounds.Width*(chart.xmax-chart.xmin);
        line(g,PointF(mouse.X,chart.bounds.Y),PointF(mouse.X,chart.bounds.GetBottom()),theme::dim,1,true);
        std::vector<wstring> rows={L"a = "+fmt(x)+L" mm"};
        for(const auto& series:chart.series) {
            auto it=std::lower_bound(series.points.begin(),series.points.end(),x,[](Sample a,double b){return a.x<b;});
            double y=it==series.points.end()?series.points.back().y:it->y;
            if(it!=series.points.begin()&&it!=series.points.end()) {auto previous=it-1;double t=(x-previous->x)/(it->x-previous->x);y=previous->y+t*(it->y-previous->y);}
            rows.push_back(series.title+L": "+fmt(y));
        }
        float boxW=std::min(228.0f,chart.bounds.Width-6),boxH=rows.size()*20.0f+14;
        float boxX=std::clamp(mouse.X+14,chart.bounds.X+3,chart.bounds.GetRight()-boxW-3);
        float boxY=std::clamp(mouse.Y+12,chart.bounds.Y+3,std::max(chart.bounds.Y+3,chart.bounds.GetBottom()-boxH-3));
        fill(g,RectF(boxX,boxY,boxW,boxH),theme::panel2);
        for(std::size_t i=0;i<rows.size();++i) label(g,rows[i],boxX+8,boxY+6+i*20,theme::text,11,false,boxW-16);
    }
}
Projected App::project(V3 v,const RectF& area) const {
    double radians=pi/180,yr=yaw*radians,tr=tilt*radians;
    double rx=v.x*std::cos(yr)-v.y*std::sin(yr),ry=v.x*std::sin(yr)+v.y*std::cos(yr);
    double sy=ry*std::cos(tr)-v.z*std::sin(tr),depth=ry*std::sin(tr)+v.z*std::cos(tr);
    double factor=std::min(area.Width/6.5,area.Height<420?std::max(90.0,area.Height-135.0)/4.7:area.Height/5.2)*zoom;
    double cy=area.Height<420?102+(area.Height-172)/2:area.Height*.55;
    return {PointF(static_cast<float>(area.X+area.Width/2+rx*factor+panX),
                   static_cast<float>(area.Y+cy-sy*factor+panY)),depth};
}
void App::poly3(Graphics& g,const std::vector<V3>& vertices,const RectF& area,Color color,Color outline) {
    std::vector<PointF> points;for(auto v:vertices)points.push_back(project(v,area).point);
    SolidBrush brush(color);Pen pen(outline,1);g.FillPolygon(&brush,points.data(),static_cast<int>(points.size()));
    g.DrawPolygon(&pen,points.data(),static_cast<int>(points.size()));
}
void App::line3(Graphics& g,V3 a,V3 b,const RectF& area,Color color,float thickness,bool arrow,bool both) {
    PointF pa=project(a,area).point,pb=project(b,area).point;line(g,pa,pb,color,thickness);
    auto head=[&](PointF tip,PointF from) {
        double angle=std::atan2(tip.Y-from.Y,tip.X-from.X);float length=8;
        PointF points[]={tip,PointF(tip.X-length*static_cast<float>(std::cos(angle-.36)),tip.Y-length*static_cast<float>(std::sin(angle-.36))),
                            PointF(tip.X-length*static_cast<float>(std::cos(angle+.36)),tip.Y-length*static_cast<float>(std::sin(angle+.36)))};
        SolidBrush brush(color);g.FillPolygon(&brush,points,3);
    };
    if(arrow||both)head(pb,pa);
    if(both)head(pa,pb);
}
void App::label3(Graphics& g,V3 v,const RectF& area,const wstring& text,Color color) {
    PointF point=project(v,area).point;label(g,text,point.X-100,point.Y-8,color,11,false,200,StringAlignmentCenter);
}
void App::drawModel(Graphics& g,const RectF& area) {
    Color color=q.classification==Classification::subcritical?theme::teal:q.classification==Classification::critical?theme::gold:theme::red;
    label(g,L"中心贯穿裂纹 · Ⅰ型张开",22,area.Y+15,theme::text,17,true,area.Width-40);
    label(g,L"均匀远场拉应力 σ  |  全长 2a  |  板厚 B",22,area.Y+45,theme::dim,11,false,area.Width-40);
    float badgeY=area.Width<670?75:19;
    label(g,q.classification==Classification::subcritical?L"亚临界 · 扩展能量不足":
          q.classification==Classification::critical?L"临界 · 能量变化率平衡":L"超临界 · 具备扩展驱动力",
          area.Width-252,area.Y+badgeY,color,11,true,230,StringAlignmentFar);
    label(g,L"G / (2γ) = "+fixed(q.ratio,3),area.Width-252,area.Y+badgeY+24,color,12,false,230,StringAlignmentFar);
    GraphicsState saved=g.Save();g.SetClip(RectF(area.X,area.Y+100,area.Width,std::max(20.0f,area.Height-168)));
    for(int i=-5;i<=5;++i) {
        double v=i*.5;line3(g,{v,-2.4,-.32},{v,2.4,-.32},area,Color(255,33,49,71));
        line3(g,{-2.5,v,-.32},{2.5,v,-.32},area,Color(255,33,49,71));
    }
    double x0=-1.85,x1=1.85,y0=-1.28,y1=1.28,d=.16;
    struct Face { std::vector<V3> points; Color color; double depth=0; };
    std::vector<Face> faces={
        {{{x0,y0,0},{x1,y0,0},{x1,y0,-d},{x0,y0,-d}},Color(255,38,55,76),0},
        {{{x1,y0,0},{x1,y1,0},{x1,y1,-d},{x1,y0,-d}},Color(255,29,46,67),0},
        {{{x0,y1,0},{x1,y1,0},{x1,y1,-d},{x0,y1,-d}},Color(255,41,61,86),0},
        {{{x0,y0,0},{x0,y1,0},{x0,y1,-d},{x0,y0,-d}},Color(255,34,50,73),0},
        {{{x0,y0,0},{x1,y0,0},{x1,y1,0},{x0,y1,0}},Color(255,52,75,103),0}};
    for(auto& face:faces)for(auto v:face.points)face.depth+=project(v,area).depth/face.points.size();
    std::sort(faces.begin(),faces.end(),[](const Face& a,const Face& b){return a.depth<b.depth;});
    for(const auto& face:faces)poly3(g,face.points,area,face.color,Color(255,99,128,159));
    double half=std::min(1.42,1.42*p.a_mm/halfspan),gap=.052,bevel=std::min(.065,half*.25);
    poly3(g,{{-half,0,.012},{-half+bevel,gap,.012},{half-bevel,gap,.012},{half,0,.012},{half-bevel,-gap,.012},{-half+bevel,-gap,.012}},area,theme::bg,theme::gold);
    for(int sign:{-1,1}) {
        poly3(g,{{-half+bevel,sign*gap,0},{half-bevel,sign*gap,0},{half-bevel,sign*gap,-d},{-half+bevel,sign*gap,-d}},area,
              sign==1?Color(255,25,38,54):Color(255,33,56,76),Color(255,96,112,134));
    }
    line3(g,{-half,0,-d},{half,0,-d},area,Color(255,183,150,80));
    for(int sign:{-1,1}) {
        PointF tip=project({sign*half,0,.016},area).point;SolidBrush brush(theme::orange);g.FillEllipse(&brush,tip.X-4,tip.Y-4,8.0f,8.0f);
        label3(g,{sign*half,-.23,.02},area,L"裂尖",theme::orange);
    }
    line3(g,{-half,.33,.01},{half,.33,.01},area,theme::gold,1.4f,true,true);
    label3(g,{0,.49,.02},area,L"2a = "+fmt(2*p.a_mm)+L" mm",theme::gold);
    for(int sign:{-1,1}) {
        for(double x:{-1.35,-.65,0.,.65,1.35}) line3(g,{x,sign*1.38,0},{x,sign*1.95,0},area,theme::blue,1.8f,true);
        label3(g,{0,sign*2.13,.01},area,L"σ = "+fmt(p.sigma_mpa)+L" MPa",theme::blue);
    }
    line3(g,{1.94,y0,0},{1.94,y0,-d},area,theme::dim,1,true,true);
    auto bp=project({2.05,y0,-d/2},area).point;label(g,L"B="+fmt(p.thickness_mm)+L" mm",bp.X+3,bp.Y-5,theme::dim,10);
    line3(g,{-2.2,-1.8,-.3},{-1.6,-1.8,-.3},area,theme::red,1.6f,true);
    line3(g,{-2.2,-1.8,-.3},{-2.2,-1.2,-.3},area,theme::teal,1.6f,true);
    line3(g,{-2.2,-1.8,-.3},{-2.2,-1.8,.35},area,theme::blue,1.6f,true);
    label3(g,{-1.5,-1.8,-.3},area,L"x",theme::red);label3(g,{-2.2,-1.09,-.3},area,L"y",theme::teal);
    label3(g,{-2.2,-1.8,.45},area,L"z",theme::blue);g.Restore(saved);
    label(g,L"局部示意；裂纹显示范围 ±"+fmt(halfspan)+L" mm；开口、厚度放大。",20,area.GetBottom()-49,theme::dim,10,false,area.Width-40);
    label(g,L"左键旋转 · 右键平移 · 滚轮缩放",20,area.GetBottom()-26,theme::blue,11,false,area.Width-40);
}
void App::paintCanvas(HDC supplied) {
    Surface surface(canvas,scale,supplied);auto& g=surface.graphics();fill(g,RectF(0,0,canvasW,canvasH),theme::bg);
    if(tab==0) drawModel(g,RectF(0,0,canvasW,std::max(160.0f,canvasH-252)));
    for(auto& chart:charts) drawChart(g,chart);
    if(tab==1)line(g,PointF(0,canvasH/2),PointF(canvasW,canvasH/2),theme::line);
    if(tab==2) {
        float y=canvasH-144;fill(g,RectF(0,y,canvasW,144),theme::panel);
        wrapped(g,L"当前半长 a = "+fmt(p.a_mm)+L" mm；全长 2a = "+fmt(2*p.a_mm)+L" mm\n"+
                  L"剩余强度 σc = "+fmt(q.sigma_c_mpa)+L" MPa；外加应力 σ = "+fmt(p.sigma_mpa)+L" MPa\n"+
                  L"保持外加应力不变时，临界半长 ac = "+fmt(q.a_c_mm)+L" mm\n\n"+
                  L"扫描只展示参数与临界条件的关系，不代表裂纹随真实时间演化。",RectF(22,y+16,canvasW-44,116),theme::dim,12);
    }
}

void App::scrollLeft(int code,int position) {
    switch(code) {
        case SB_LINEUP:leftOffset-=35;break;case SB_LINEDOWN:leftOffset+=35;break;
        case SB_PAGEUP:leftOffset-=static_cast<int>(canvasH)-35;break;
        case SB_PAGEDOWN:leftOffset+=static_cast<int>(canvasH)-35;break;
        case SB_THUMBTRACK:case SB_THUMBPOSITION:leftOffset=position;break;
        case SB_TOP:leftOffset=0;break;case SB_BOTTOM:leftOffset=990;break;
    }
    updateScroll();layoutLeft();
}
void App::sliderChanged(int index,double value) {
    stopScan();error.clear();
    value=std::clamp(value,minima[index],maxima[index]);
    std::wostringstream text;text<<std::setprecision(8)<<value;SetWindowTextW(edits[index],text.str().c_str());
    InvalidateRect(sliders[index],nullptr,FALSE);SetTimer(window,1,60,nullptr);
}
void App::pick(double a) {
    stopScan();std::wostringstream text;text<<std::setprecision(10)<<std::max(a,1e-6);
    SetWindowTextW(edits[3],text.str().c_str());update();
}
void App::mouseDown(PointF point,int kind) {
    SetFocus(canvas);
    for(const auto& chart:charts) if(chart.bounds.Contains(point)) {
        if(kind==1)pick(chart.xmin+(point.X-chart.bounds.X)/chart.bounds.Width*(chart.xmax-chart.xmin));
        return;
    }
    if(tab==0&&point.Y<canvasH-252) {dragKind=kind;dragPoint=point;SetCapture(canvas);hover=false;}
}
void App::mouseMove(PointF point) {
    mouse=point;
    if(dragKind) {
        float dx=point.X-dragPoint.X,dy=point.Y-dragPoint.Y;
        if(dragKind==1) {yaw+=dx*.45;tilt=std::clamp(tilt+dy*.4,-85.,85.);}
        else {panX+=dx;panY+=dy;}
        dragPoint=point;
    } else {
        hover=true;TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,canvas,0};TrackMouseEvent(&track);
    }
    InvalidateRect(canvas,nullptr,FALSE);
}
void App::mouseUp() {if(dragKind){dragKind=0;ReleaseCapture();}}
void App::wheel(int delta,PointF point) {
    if(tab==0&&point.Y<canvasH-252) {zoom=std::clamp(zoom*std::pow(1.08,delta/120.0),.55,2.);InvalidateRect(canvas,nullptr,FALSE);}
}
bool App::writeCsv(const fs::path& destination) {
    auto rows=curve_points(p,maxA(),241);
    std::ostringstream stream;stream<<std::setprecision(17);
    stream<<"Griffith infinite plate, center through-crack, half-length a\r\n";
    stream<<"E_GPa,"<<p.E_gpa<<",nu,"<<p.nu<<",gamma_J_m2,"<<p.gamma<<",sigma_MPa,"<<p.sigma_mpa
          <<",B_mm,"<<p.thickness_mm<<",state,"<<state_name(p.state)<<"\r\n";
    stream<<"a_mm,U_release_j,S_surface_j,net_release_j,G_j_m2,R_j_m2,delta_total_energy_J,sigma_c_MPa\r\n";
    for(const auto& row:rows) {
        stream<<row.a_mm<<','<<row.U_release_j<<','<<row.S_surface_j<<','<<row.net_release_j<<','
              <<row.G_j_m2<<','<<row.R_j_m2<<','<<-row.net_release_j<<',';
        if(row.a_mm>0)stream<<std::sqrt(2*q.E_prime_pa*p.gamma/(pi*(row.a_mm/1000)))/1e6;else stream<<"infinity";
        stream<<"\r\n";
    }
    FILE* file=_wfopen(destination.c_str(),L"wb");if(!file)return false;
    const std::string text="\xef\xbb\xbf"+stream.str();
    bool ok=std::fwrite(text.data(),1,text.size(),file)==text.size();if(std::fclose(file)!=0)ok=false;
    return ok;
}
void App::exportCsv() {
    if(!update(true)) return;
    wchar_t filename[32768]=L"Griffith扫描数据.csv";
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;
    dialog.lpstrFilter=L"CSV 数据 (*.csv)\0*.csv\0所有文件 (*.*)\0*.*\0\0";
    dialog.lpstrFile=filename;dialog.nMaxFile=32768;dialog.lpstrTitle=L"导出 Griffith 扫描数据";
    dialog.lpstrDefExt=L"csv";dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    if(writeCsv(fs::path(filename)))status=L"扫描数据已导出："+fs::path(filename).filename().wstring();
    else {status=L"无法写入 CSV，请检查保存位置。";MessageBoxW(window,status.c_str(),L"导出失败",MB_OK|MB_ICONERROR);}
    InvalidateRect(window,nullptr,FALSE);
}
void App::showHelp() {
    const wchar_t* text=L"Griffith 能量平衡原理\n\n"
        L"无限大板中心贯穿裂纹；裂纹全长 2a，板厚 B。\n"
        L"Urel = πa²σ²B / E′；S = 4aBγ = 2γA；A = 2aB。\n"
        L"G = πaσ²/E′；R = 2γ；G = R 为扩展临界条件。\n"
        L"σc = √(2E′γ/πa)；ac = 2E′γ/(πσ²)。\n"
        L"E′ = E（平面应力）；E′ = E/(1−ν²)（平面应变）。\n\n"
        L"临界条件比较能量变化率：a=ac 时 Urel=S/2；\n"
        L"Urel=S 的总能量交点在 a=2ac，不是起裂临界点。\n"
        L"恒载时外力继续做功，应变能可以增加；Urel 为外力功\n"
        L"扣除应变能变化后的有效释放能量。\n\n"
        L"输入：E、γ、a、B > 0；σ ≥ 0；0 ≤ ν < 0.5。\n"
        L"滑块为快捷范围，输入框可填写范围外的合法数值。\n"
        L"Enter 更新；左键旋转、右键平移、滚轮缩放；\n"
        L"点击曲线选择半长 a；Esc 停止扫描；支持 CSV 导出。\n\n"
        L"扫描展示参数关系，不代表裂纹随真实时间扩展。\n"
        L"本模型不包含塑性功、有限宽修正、疲劳或扩展速率。\n"
        L"有限试样应结合加载约束与柔度分析；起裂判据\n"
        L"不单独给出持续失稳或最终裂纹长度。";
    MessageBoxW(window,text,L"公式、操作与模型说明",MB_OK|MB_ICONINFORMATION);
}

App* bindApp(HWND hwnd,UINT message,LPARAM lparam) {
    if(message==WM_NCCREATE) {
        auto* create=reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    return reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
}
LRESULT CALLBACK MainProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    App* app=bindApp(hwnd,message,lp);if(!app)return DefWindowProcW(hwnd,message,wp,lp);
    switch(message) {
        case WM_CREATE:app->window=hwnd;try{app->createControls();}catch(...){return -1;}return 0;
        case WM_SIZE:if(app->left)app->layout();return 0;
        case WM_GETMINMAXINFO:{auto* info=reinterpret_cast<MINMAXINFO*>(lp);info->ptMinTrackSize={app->px(1080),app->px(680)};return 0;}
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:app->paintRoot();return 0;
        case WM_PRINTCLIENT:app->paintRoot(reinterpret_cast<HDC>(wp));return 0;
        case WM_COMMAND:app->command(LOWORD(wp),HIWORD(wp));return 0;
        case WM_DRAWITEM:app->drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;
        case WM_TIMER:if(wp==1)app->update();else if(wp==2)app->scanStep();return 0;
        case WM_CLOSE:app->stopScan();DestroyWindow(hwnd);return 0;
        case WM_DESTROY:KillTimer(hwnd,1);KillTimer(hwnd,2);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
LRESULT CALLBACK LeftProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    App* app=bindApp(hwnd,message,lp);if(!app)return DefWindowProcW(hwnd,message,wp,lp);
    switch(message) {
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:app->paintLeft();return 0;
        case WM_PRINTCLIENT:app->paintLeft(reinterpret_cast<HDC>(wp));return 0;
        case WM_DRAWITEM:app->drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;
        case WM_COMMAND:app->command(LOWORD(wp),HIWORD(wp));return 0;
        case WM_CTLCOLOREDIT:case WM_CTLCOLORSTATIC:
            SetTextColor(reinterpret_cast<HDC>(wp),rgb(theme::text));SetBkColor(reinterpret_cast<HDC>(wp),rgb(theme::bg));return reinterpret_cast<LRESULT>(app->editBrush);
        case WM_VSCROLL:{SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_TRACKPOS;GetScrollInfo(hwnd,SB_VERT,&info);app->scrollLeft(LOWORD(wp),info.nTrackPos);return 0;}
        case WM_MOUSEWHEEL:app->leftOffset-=GET_WHEEL_DELTA_WPARAM(wp)/120*70;app->updateScroll();app->layoutLeft();return 0;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
LRESULT CALLBACK RightProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    App* app=bindApp(hwnd,message,lp);if(!app)return DefWindowProcW(hwnd,message,wp,lp);
    switch(message) {
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:app->paintRight();return 0;
        case WM_PRINTCLIENT:app->paintRight(reinterpret_cast<HDC>(wp));return 0;
        case WM_MOUSEWHEEL:return SendMessageW(app->results,message,wp,lp);
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
LRESULT CALLBACK CanvasProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    App* app=bindApp(hwnd,message,lp);if(!app)return DefWindowProcW(hwnd,message,wp,lp);
    PointF point(GET_X_LPARAM(lp)/app->scale,GET_Y_LPARAM(lp)/app->scale);
    switch(message) {
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:app->paintCanvas();return 0;
        case WM_PRINTCLIENT:app->paintCanvas(reinterpret_cast<HDC>(wp));return 0;
        case WM_LBUTTONDOWN:app->mouseDown(point,1);return 0;
        case WM_RBUTTONDOWN:app->mouseDown(point,2);return 0;
        case WM_MOUSEMOVE:app->mouseMove(point);return 0;
        case WM_LBUTTONUP:case WM_RBUTTONUP:app->mouseUp();return 0;
        case WM_CAPTURECHANGED:app->dragKind=0;return 0;
        case WM_MOUSELEAVE:app->hover=false;InvalidateRect(hwnd,nullptr,FALSE);return 0;
        case WM_MOUSEWHEEL:{POINT cursor{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(hwnd,&cursor);
            app->wheel(GET_WHEEL_DELTA_WPARAM(wp),PointF(cursor.x/app->scale,cursor.y/app->scale));return 0;}
        case WM_SETCURSOR:SetCursor(LoadCursorW(nullptr,app->tab==0?IDC_SIZEALL:IDC_CROSS));return TRUE;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
LRESULT CALLBACK SliderProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_NCCREATE)SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
    auto* data=reinterpret_cast<SliderData*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(!data)return DefWindowProcW(hwnd,message,wp,lp);
    App* app=data->app;int index=data->index;
    auto value=[&]() {wchar_t text[128]{};GetWindowTextW(app->edits[index],text,128);return std::wcstod(text,nullptr);};
    auto choose=[&](int x) {RECT r{};GetClientRect(hwnd,&r);double ratio=std::clamp((x-app->px(6))/static_cast<double>(std::max(1L,r.right-app->px(12))),0.,1.);
        app->sliderChanged(index,minima[index]+ratio*(maxima[index]-minima[index]));};
    switch(message) {
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:case WM_PRINTCLIENT:{
            Surface surface(hwnd,app->scale,message==WM_PRINTCLIENT?reinterpret_cast<HDC>(wp):nullptr);auto& g=surface.graphics();
            RECT r{};GetClientRect(hwnd,&r);float w=r.right/app->scale,h=r.bottom/app->scale;
            fill(g,RectF(0,0,w,h),theme::panel);
            double v=value();if(!std::isfinite(v))v=minima[index];double fraction=std::clamp((v-minima[index])/(maxima[index]-minima[index]),0.,1.);
            float x=6+static_cast<float>(fraction)*(w-12);
            line(g,PointF(6,h/2),PointF(w-6,h/2),theme::bg,5);
            line(g,PointF(6,h/2),PointF(x,h/2),Color(255,53,93,137),5);
            fill(g,RectF(x-5,h/2-6,10,12),GetFocus()==hwnd?theme::blue:Color(255,88,139,199));return 0;
        }
        case WM_LBUTTONDOWN:SetFocus(hwnd);SetCapture(hwnd);choose(GET_X_LPARAM(lp));return 0;
        case WM_MOUSEMOVE:if(GetCapture()==hwnd)choose(GET_X_LPARAM(lp));return 0;
        case WM_LBUTTONUP:if(GetCapture()==hwnd){choose(GET_X_LPARAM(lp));ReleaseCapture();}return 0;
        case WM_KEYDOWN:{double delta=(maxima[index]-minima[index])/200;
            if(wp==VK_LEFT||wp==VK_DOWN)app->sliderChanged(index,value()-delta);
            else if(wp==VK_RIGHT||wp==VK_UP)app->sliderChanged(index,value()+delta);
            else if(wp==VK_HOME)app->sliderChanged(index,minima[index]);
            else if(wp==VK_END)app->sliderChanged(index,maxima[index]);
            return 0;}
        case WM_GETDLGCODE:return DLGC_WANTARROWS;
        case WM_SETFOCUS:case WM_KILLFOCUS:InvalidateRect(hwnd,nullptr,FALSE);return 0;
        case WM_MOUSEWHEEL:app->leftOffset-=GET_WHEEL_DELTA_WPARAM(wp)/120*70;app->updateScroll();app->layoutLeft();return 0;
        case WM_SETCURSOR:SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}

bool App::capture(const fs::path& destination) {
    RECT r{};GetClientRect(window,&r);HDC dc=GetDC(window),memory=CreateCompatibleDC(dc);
    HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom),old=static_cast<HBITMAP>(SelectObject(memory,bitmap));
    // Print each native child into its own clipped rectangle. Printing the whole
    // top-level window would add the caption offset even to a client-only image.
    paintRoot(memory);
    std::function<void(HWND)> render=[&](HWND target) {
        RECT rect{};GetWindowRect(target,&rect);POINT origin{rect.left,rect.top};ScreenToClient(window,&origin);
        int state=SaveDC(memory);SetViewportOrgEx(memory,origin.x,origin.y,nullptr);
        IntersectClipRect(memory,0,0,rect.right-rect.left,rect.bottom-rect.top);
        wchar_t className[64]{};GetClassNameW(target,className,64);
        if(std::wcscmp(className,L"Edit")==0) {
            // A hidden ancestor prevents single-line EDIT text from printing.
            // Reproduce its current text, font and colors for the smoke image.
            Graphics g(memory);g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);g.ScaleTransform(scale,scale);
            float w=(rect.right-rect.left)/scale,h=(rect.bottom-rect.top)/scale;
            fill(g,RectF(0,0,w,h),theme::bg);Pen border(theme::line,1);g.DrawRectangle(&border,.5f,.5f,w-1,h-1);
            wchar_t value[128]{};GetWindowTextW(target,value,128);Font f(L"Consolas",14,FontStyleRegular,UnitPixel);
            SolidBrush brush(theme::text);StringFormat format;format.SetAlignment(StringAlignmentFar);format.SetLineAlignment(StringAlignmentCenter);
            format.SetFormatFlags(StringFormatFlagsNoWrap);g.DrawString(value,-1,&f,RectF(5,0,w-10,h),&format,&brush);
        } else SendMessageW(target,WM_PRINT,reinterpret_cast<WPARAM>(memory),PRF_NONCLIENT|PRF_CLIENT|PRF_ERASEBKGND);
        for(HWND child=GetWindow(target,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
            if(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE)render(child);
        RestoreDC(memory,state);
    };
    for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
        if(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE)render(child);
    bool ok=false;
    {
        Bitmap image(bitmap,nullptr);UINT count=0,bytes=0;GetImageEncodersSize(&count,&bytes);std::vector<BYTE> buffer(bytes);
        auto* encoders=reinterpret_cast<ImageCodecInfo*>(buffer.data());GetImageEncoders(count,bytes,encoders);
        for(UINT i=0;i<count;++i)if(std::wcscmp(encoders[i].MimeType,L"image/png")==0) {
            ok=image.Save(destination.c_str(),&encoders[i].Clsid,nullptr)==Ok;break;
        }
    }
    SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(window,dc);return ok;
}
int App::smokeTest(const fs::path& directory) {
    fs::create_directories(directory);std::vector<std::string> passed;
    auto require=[&](bool condition,const char* message) {if(!condition)throw std::runtime_error(message);passed.push_back(message);};
    int code=0;std::string failure;
    try {
        require(left&&right&&canvas&&results,"Native controls created");
        require(q.classification==Classification::subcritical,"Default subcritical state");
        for(int i=0;i<3;++i) {
            SendMessageW(window,WM_COMMAND,Tab0+i,0);
            const wchar_t* files[]={L"界面预览.png",L"能量曲线预览.png",L"临界应力扫描预览.png"};
            require(capture(directory/files[i]),"Page PNG capture");
            require(charts.size()==static_cast<std::size_t>(i==1?2:1),"Page chart count");
        }
        const Classification expected[]={Classification::subcritical,Classification::critical,Classification::supercritical,Classification::subcritical};
        for(int i=0;i<4;++i) {SendMessageW(window,WM_COMMAND,Example0+i,0);require(q.classification==expected[i],"Teaching example classification");}
        SendMessageW(window,WM_COMMAND,Defaults,0);SetWindowTextW(edits[3],L"10");require(update(),"Direct entry above slider range accepted");
        require(p.a_mm==10,"Direct entry remains unclamped");SetWindowTextW(edits[4],L"0");require(update(),"Zero stress accepted");
        require(q.G_j_m2==0&&std::isinf(q.a_c_mm),"Zero stress has infinite critical half-length");
        SetWindowTextW(edits[1],L"0.5");require(!update(),"Nonphysical input rejected");
        require(p.nu==.22,"Invalid input preserves accepted state");
        SendMessageW(window,WM_COMMAND,Defaults,0);
        require(writeCsv(directory/L"示例扫描数据.csv"),"UTF-8 CSV export to Chinese path");
        require(fs::file_size(directory/L"示例扫描数据.csv")>1000,"CSV contains scan data");
        SendMessageW(sliders[3],WM_KEYDOWN,VK_RIGHT,0);require(update(),"Slider keyboard update");
        require(p.a_mm>.5,"Slider changes input");
        SendMessageW(window,WM_COMMAND,Defaults,0);require(startScan(),"Scan starts");
        const double fixedMax=scanLimit;for(int i=0;i<5;++i)SendMessageW(window,WM_TIMER,2,0);
        require(scanning&&scanIndex==5&&maxA()==fixedMax,"Scan keeps plot scale fixed");
        stopScan();require(!scanning,"Scan stops");
        SendMessageW(window,WM_COMMAND,Defaults,0);SendMessageW(window,WM_COMMAND,Front,0);
        require(yaw==0&&tilt==0,"Front view preset");SendMessageW(window,WM_COMMAND,Top,0);require(tilt==76,"Top view preset");
        SendMessageW(window,WM_COMMAND,Iso,0);require(yaw==-26&&tilt==38,"Isometric view preset");
        mouseDown(PointF(200,180),1);mouseMove(PointF(230,200));mouseUp();require(yaw!=-26&&tilt!=38,"Model rotation");
        mouseDown(PointF(200,180),2);mouseMove(PointF(220,190));mouseUp();require(panX==20&&panY==10,"Model pan");
        wheel(120,PointF(200,180));require(zoom>1,"Model wheel zoom");
        preset(Iso);selectTab(0);require(capture(directory/L"界面预览.png"),"Model page final capture");
        require(!charts.empty()&&charts[0].bounds.Width>0,"Chart bounds computed");
        RectF bounds=charts[0].bounds;double previous=p.a_mm;
        mouseDown(PointF(bounds.X+bounds.Width*.8,bounds.Y+bounds.Height*.5),1);
        require(p.a_mm!=previous,"Chart click selects crack half-length");
        scrollLeft(SB_BOTTOM);require(leftOffset>0,"Input panel scroll");scrollLeft(SB_TOP);
        require(GetWindowTextLengthW(results)>500,"Formula and results text populated");
        example(0);preset(Iso);for(int i=0;i<3;++i) {selectTab(i);const wchar_t* files[]={L"界面预览.png",L"能量曲线预览.png",L"临界应力扫描预览.png"};require(capture(directory/files[i]),"Final default page capture");}
    } catch(const std::exception& exc) {code=1;failure=exc.what();}
    std::ostringstream report;report<<(code==0?"PASS":"FAIL")<<": C++ native GUI smoke test\r\n";
    report<<passed.size()<<" checks passed.\r\n";for(const auto& text:passed)report<<"PASS: "<<text<<"\r\n";
    if(code)report<<"FAIL: "<<failure<<"\r\n";
    FILE* file=_wfopen((directory/L"界面验证结果.txt").c_str(),L"wb");
    if(file){auto text=report.str();std::fwrite(text.data(),1,text.size(),file);std::fclose(file);}else code=1;
    DestroyWindow(window);return code;
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
    HMODULE rich=LoadLibraryW(L"Msftedit.dll");if(!rich)return 1;
    GdiplusStartupInput input;ULONG_PTR token=0;if(GdiplusStartup(&token,&input,nullptr)!=Ok)return 1;
    int count=0;LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count);
    bool smoke=count>1&&std::wcscmp(args[1],L"--smoke-test")==0;
    fs::path directory=count>2?fs::path(args[2]):fs::current_path();
    LocalFree(args);int code=0;
    try {
        App app(instance);app.smokeMode=smoke;
        const wchar_t* names[]={L"GriffithMain",L"GriffithLeft",L"GriffithRight",L"GriffithCanvas",L"GriffithSlider"};
        const WNDPROC procedures[]={MainProc,LeftProc,RightProc,CanvasProc,SliderProc};
        for(int i=0;i<5;++i) {
            WNDCLASSEXW cls{};cls.cbSize=sizeof(cls);cls.style=CS_HREDRAW|CS_VREDRAW;cls.lpfnWndProc=procedures[i];cls.hInstance=instance;
            cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.lpszClassName=names[i];cls.hIcon=LoadIconW(nullptr,IDI_APPLICATION);RegisterClassExW(&cls);
        }
        int w=app.px(std::min(1680.0f,GetSystemMetrics(SM_CXSCREEN)/app.scale-44));
        int h=app.px(std::min(980.0f,GetSystemMetrics(SM_CYSCREEN)/app.scale-70));
        RECT bounds{0,0,w,h};AdjustWindowRectEx(&bounds,WS_OVERLAPPEDWINDOW,FALSE,0);
        HWND hwnd=CreateWindowExW(0,L"GriffithMain",L"Griffith 能量平衡原理 - C++ 计算与可视化",WS_OVERLAPPEDWINDOW,
                                  app.px(18),app.px(12),bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,instance,&app);
        if(!hwnd)throw std::runtime_error("Could not create the application window.");
        if(smoke)code=app.smokeTest(directory);
        else {
            ShowWindow(hwnd,show);UpdateWindow(hwnd);MSG message{};
            while(GetMessageW(&message,nullptr,0,0)>0) {
                if(message.message==WM_KEYDOWN&&message.wParam==VK_RETURN) {app.stopScan();app.update(true);continue;}
                if(message.message==WM_KEYDOWN&&message.wParam==VK_ESCAPE) {app.stopScan();app.update();continue;}
                if(!IsDialogMessageW(hwnd,&message)){TranslateMessage(&message);DispatchMessageW(&message);}
            }
        }
    } catch(const std::exception& exc) {
        code=1;if(!smoke)MessageBoxW(nullptr,widen(exc.what()).c_str(),L"Griffith 启动失败",MB_OK|MB_ICONERROR);
    }
    GdiplusShutdown(token);FreeLibrary(rich);return code;
}
