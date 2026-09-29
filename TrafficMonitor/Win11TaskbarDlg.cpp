#include "stdafx.h"
#include "Win11TaskbarDlg.h"
#include "WindowsSettingHelper.h"

namespace
{
    bool IsSameRect(const CRect& left, const CRect& right)
    {
        return left.left == right.left && left.top == right.top
            && left.right == right.right && left.bottom == right.bottom;
    }

    void WriteTaskbarDebugLog(const CString& message)
    {
        if (theApp.m_debug_log)
            CCommon::WriteLog(message, (theApp.m_config_dir + L".\\debug.log").c_str());
    }
}

void CWin11TaskbarDlg::AdjustTaskbarWndPos(bool force_adjust)
{
    CRect current_notify{};
    CRect current_start{};
    const bool notify_rect_valid = ::GetWindowRect(m_hNotify, current_notify);
    const bool start_rect_valid = ::GetWindowRect(m_hStart, current_start);
    if (!notify_rect_valid)
        current_notify.SetRectEmpty();
    if (!start_rect_valid)
    {
        m_layout_adjustment_deferred = true;
        WriteTaskbarDebugLog(_T("Win11 taskbar layout skipped: Start window rectangle is unavailable."));
        return;
    }
    m_rcNotify = current_notify;
    m_rcStart = current_start;
    m_rcStart.MoveToXY(m_rcStart.left - m_rcTaskbar.left, m_rcStart.top - m_rcTaskbar.top);

    //设置窗口大小
    m_rect.right = m_rect.left + m_window_width;
    m_rect.bottom = m_rect.top + m_window_height;
    const bool geometry_changed = !m_layout_initialized
        || !IsSameRect(m_rcNotify, m_last_notify_rect)
        || !IsSameRect(m_rcStart, m_last_start_rect)
        || !IsSameRect(m_rcTaskbar, m_last_taskbar_rect)
        || m_taskbar_dpi != m_last_layout_dpi;
    if (force_adjust || geometry_changed)
    {
        CString debug_info;
        debug_info.Format(_T("Win11 taskbar layout: force=%d geometry=%d dpi=%u taskbar=(%d,%d)-(%d,%d) notify=(%d,%d)-(%d,%d) start=(%d,%d)-(%d,%d) window=(%d,%d)-(%d,%d)"),
            force_adjust, geometry_changed, m_taskbar_dpi,
            m_rcTaskbar.left, m_rcTaskbar.top, m_rcTaskbar.right, m_rcTaskbar.bottom,
            m_rcNotify.left, m_rcNotify.top, m_rcNotify.right, m_rcNotify.bottom,
            m_rcStart.left, m_rcStart.top, m_rcStart.right, m_rcStart.bottom,
            m_rect.left, m_rect.top, m_rect.right, m_rect.bottom);
        WriteTaskbarDebugLog(debug_info);
        m_last_notify_rect = m_rcNotify;
        m_last_start_rect = m_rcStart;
        m_last_taskbar_rect = m_rcTaskbar;
        m_last_layout_dpi = m_taskbar_dpi;
        m_layout_initialized = true;
        //任务窗口显示在右侧时，或者Windows11下任务栏左对齐时
        //（Windows11下，如果任务栏设置为左对齐，即使在“任务栏窗口设置”中设置了任务窗口显示在左边，窗口仍然显示在右边）
        if (!theApp.m_taskbar_data.tbar_wnd_on_left || !CWindowsSettingHelper::IsTaskbarCenterAlign())
        {
            ////靠近任务栏图标的情况
            //if (theApp.m_taskbar_data.tbar_wnd_snap && IsTaskbarCloseToIconEnable(theApp.m_taskbar_data.tbar_wnd_on_left))
            //{
            //    m_rect.MoveToX(m_rcMin.right + 2);
            //}
            ////靠近通知区的情况
            //else
            //{
            //通知区窗口的水平位置
            int notify_x_pos = m_rcNotify.left;
            //没有获取到通知区位置的情况
            if (notify_x_pos == 0)
            {
                //Win11副屏没有通知区窗口，这里使用固定的值（88像素的系统时间区域）
                if (m_is_secondary_display)
                    notify_x_pos = m_rcTaskbar.Width() - DPI(88);
                //如果不是副屏，但是仍然没有获取到通知区域的位置，使用配置文件中taskbar_right_space_win11指定的值
                else
                    notify_x_pos = m_rcTaskbar.Width() - DPI(theApp.m_taskbar_data.taskbar_right_space_win11);
            }
            //如果显示了小组件，并且任务栏靠左显示，则留出小组件的位置
            if (theApp.m_taskbar_data.avoid_overlap_with_widgets && CWindowsSettingHelper::IsTaskbarWidgetsBtnShown() && !CWindowsSettingHelper::IsTaskbarCenterAlign())
                m_rect.MoveToX(notify_x_pos - m_rect.Width() + 2 - DPI(theApp.m_taskbar_data.taskbar_left_space_win11));
            else
                m_rect.MoveToX(notify_x_pos - m_rect.Width() + 2);
            //}
        }
        //任务栏窗口显示在左侧时
        else
        {
            //靠近“开始”按钮
            if (theApp.m_taskbar_data.tbar_wnd_snap)
            {
                m_rect.MoveToX(m_rcStart.left - m_rect.Width() - 2);
            }
            //靠近最左侧
            else
            {
                if (CWindowsSettingHelper::IsTaskbarWidgetsBtnShown())
                    m_rect.MoveToX(2 + DPI(theApp.m_taskbar_data.taskbar_left_space_win11));
                else
                    m_rect.MoveToX(2);
            }
        }
        //水平偏移
        m_rect.MoveToX(m_rect.left + DPI(theApp.m_taskbar_data.window_offset_left));
        ////确保水平方向不超出屏幕边界
        //if (m_rect.left < 0)
        //    m_rect.MoveToX(0);
        //if (m_rcTaskbar.Width() > m_rect.Width() && m_rect.right > m_rcTaskbar.Width())
        //    m_rect.MoveToX(m_rcTaskbar.Width() - m_rect.Width());

        //设置任务栏窗口的垂直位置
        //注：这里加上(m_rcTaskbar.Height() - rcStart.Height())用于修正Windows11 build 22621版本后触屏设备任务栏窗口位置不正确的问题。
        //在这种情况下m_rcTaskbar的高度要大于m_rcBar的高度，正常情况下，它们的高度相同
        //但是当任务栏上没有任何图标时，m_rcBar的高度会变为0，因此使用rcStart代替
        m_rect.MoveToY((m_rcStart.Height() - m_rect.Height()) / 2 + (m_rcTaskbar.Height() - m_rcStart.Height()) + DPI(theApp.m_taskbar_data.window_offset_top));
        ////确保垂直方向不超出屏幕边界
        //if (m_rect.top < 0)
        //    m_rect.MoveToY(0);
        //if (m_rcTaskbar.Height() > m_rect.Height() && m_rect.bottom > m_rcTaskbar.Height())
        //    m_rect.MoveToY(m_rcTaskbar.Height() - m_rect.Height());

        MoveWindow(m_rect);
    }
}

void CWin11TaskbarDlg::InitTaskbarWnd()
{
    m_hNotify = ::FindWindowEx(m_hTaskbar, 0, L"TrayNotifyWnd", NULL);
    m_hStart = ::FindWindowEx(m_hTaskbar, nullptr, L"Start", NULL);
    ::GetWindowRect(m_hNotify, m_rcNotify);
}

bool CWin11TaskbarDlg::IsTaskbarStructureChanged()
{
    if (IsTaskbarHandleChanged())
        return true;

    HWND current_notify = ::FindWindowEx(m_hTaskbar, 0, L"TrayNotifyWnd", NULL);
    HWND current_start = ::FindWindowEx(m_hTaskbar, nullptr, L"Start", NULL);
    return current_start == NULL || current_notify != m_hNotify || current_start != m_hStart
        || !::IsWindow(m_hStart) || (m_hNotify != NULL && !::IsWindow(m_hNotify));
}

void CWin11TaskbarDlg::ResetTaskbarPos()
{
}

HWND CWin11TaskbarDlg::GetParentHwnd()
{
    return m_hTaskbar;
}

void CWin11TaskbarDlg::CheckTaskbarOnTopOrBottom()
{
    m_taskbar_on_top_or_bottom = true;
}
