// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#pragma once
#include <windows.h>
#include <UIAutomation.h>
#include <atlbase.h>
#include <cwchar>

class TaskbarSpaceProbe final
{
public:
    TaskbarSpaceProbe() = default;
    TaskbarSpaceProbe(const TaskbarSpaceProbe&) = delete;
    TaskbarSpaceProbe& operator=(const TaskbarSpaceProbe&) = delete;
    ~TaskbarSpaceProbe()
    {
        automation_.Release();
        // CoUninitialize only balances an apartment this object initialized.
        if (apartmentOwned_) CoUninitialize();
    }

    bool IsFree(HWND taskbar, RECT candidate, HWND strip = nullptr) noexcept
    {
        MapWindowPoints(taskbar, nullptr, reinterpret_cast<POINT*>(&candidate), 2);
        const HWND bridge = FindWindowExW(taskbar, nullptr, L"Windows.UI.Composition.DesktopWindowContentBridge", nullptr);
        const HWND xamlHost = bridge == nullptr ? nullptr :
            FindWindowExW(bridge, nullptr, L"Windows.UI.Input.InputSite.WindowClass", nullptr);
        if (xamlHost == nullptr) return false;
        NativeSpace native{candidate, xamlHost, strip};
        EnumChildWindows(taskbar, CheckNativeChild, reinterpret_cast<LPARAM>(&native));
        if (!native.free) return false;
        if (!automation_)
        {
            if (!apartmentOwned_)
            {
                const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                apartmentOwned_ = SUCCEEDED(initialized);
                if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return false;
            }
            if (FAILED(automation_.CoCreateInstance(__uuidof(CUIAutomation8)))) return false;
            if (FAILED(automation_->put_ConnectionTimeout(250)) ||
                FAILED(automation_->put_TransactionTimeout(250)))
            {
                automation_.Release();
                return false;
            }
        }
        CComPtr<IUIAutomationElement> root;
        CComPtr<IUIAutomationCondition> condition;
        CComPtr<IUIAutomationCacheRequest> cache;
        CComPtr<IUIAutomationElementArray> controls;
        // Query Explorer's XAML host rather than our own sibling window's UIA provider.
        if (FAILED(automation_->ElementFromHandle(xamlHost, &root)) ||
            FAILED(automation_->get_ControlViewCondition(&condition)) ||
            FAILED(automation_->CreateCacheRequest(&cache)) ||
            FAILED(cache->put_TreeScope(TreeScope_Element)) ||
            FAILED(cache->put_TreeFilter(condition))) return false;
        for (PROPERTYID property : {UIA_NativeWindowHandlePropertyId, UIA_IsOffscreenPropertyId,
            UIA_ClassNamePropertyId, UIA_AutomationIdPropertyId, UIA_BoundingRectanglePropertyId})
        {
            if (FAILED(cache->AddProperty(property))) return false;
        }
        if (FAILED(root->FindAllBuildCache(TreeScope_Descendants, condition, cache, &controls))) return false;
        int count = 0;
        if (FAILED(controls->get_Length(&count))) return false;
        bool foundFrame = false, foundStart = false;
        for (int index = 0; index < count; ++index)
        {
            CComPtr<IUIAutomationElement> control;
            BOOL offscreen = FALSE;
            UIA_HWND nativeWindow = nullptr;
            CComBSTR className, id;
            if (FAILED(controls->GetElement(index, &control)) ||
                FAILED(control->get_CachedNativeWindowHandle(&nativeWindow)) ||
                FAILED(control->get_CachedIsOffscreen(&offscreen))) return false;
            if (offscreen || (strip != nullptr && reinterpret_cast<HWND>(nativeWindow) == strip)) continue;
            if (FAILED(control->get_CachedClassName(&className)) ||
                FAILED(control->get_CachedAutomationId(&id))) return false;
            const bool structuralFrame = className && wcscmp(className, L"Taskbar.TaskbarFrameAutomationPeer") == 0;
            foundFrame |= structuralFrame;
            foundStart |= id && wcscmp(id, L"StartButton") == 0;
            // Only the verified layout frame is empty space; buttons occupy their full bounds, including padding.
            if (structuralFrame) continue;
            RECT occupied{}, overlap{};
            if (FAILED(control->get_CachedBoundingRectangle(&occupied)) || IsRectEmpty(&occupied)) return false;
            if (IntersectRect(&overlap, &candidate, &occupied)) return false;
        }
        // shortcut: recognize the Win11 XAML frame and StartButton, expand only after validating another shell tree.
        return foundFrame && foundStart;
    }

private:
    struct NativeSpace final
    {
        RECT candidate;
        HWND xamlHost, strip;
        bool free = true;
    };
    static BOOL CALLBACK CheckNativeChild(HWND child, LPARAM parameter)
    {
        auto& space = *reinterpret_cast<NativeSpace*>(parameter);
        if (child == space.strip || child == space.xamlHost || IsChild(space.xamlHost, child) || IsChild(child, space.xamlHost) ||
            !IsWindowVisible(child)) return TRUE;
        RECT occupied{}, overlap{};
        if (!GetWindowRect(child, &occupied) || IntersectRect(&overlap, &space.candidate, &occupied))
        {
            space.free = false;
            return FALSE;
        }
        return TRUE;
    }
    CComPtr<IUIAutomation2> automation_;
    bool apartmentOwned_ = false;
};
