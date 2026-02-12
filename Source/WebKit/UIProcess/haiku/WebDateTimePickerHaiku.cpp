/*
 * Copyright (C) 2019 Haiku, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebDateTimePickerHaiku.h"

#include "WebPageProxy.h"
#include <WebCore/InputTypeNames.h>
#include <wtf/text/CString.h>
#include <wtf/RunLoop.h>

#include <cstdlib>
#include <cerrno>

#include <support/Locker.h>
#include <locale/Collator.h>
#include <private/shared/CalendarView.h>
#include <LocaleRoster.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <SeparatorView.h>
#include <TextControl.h>
#include <TimeFormat.h>
#include <Button.h>
#include <GroupLayoutBuilder.h>
#include <Window.h>

namespace WebKit {
using namespace WebCore;

class DateTimeChooserWindow: public BWindow
{
public:
    DateTimeChooserWindow(WebDateTimePickerHaiku& picker)
        : BWindow(BRect(0, 0, 10, 10), "Date Picker", B_FLOATING_WINDOW,
            B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS)
        , m_picker(picker)
        , m_calendar(nullptr)
        , m_yearControl(nullptr)
        , m_okButton(nullptr)
        , m_hourMenu(nullptr)
        , m_minuteMenu(nullptr)
        , m_mainGroup(nullptr)
    {
        BGroupLayout* root = new BGroupLayout(B_VERTICAL);
        root->SetSpacing(0);
        SetLayout(root);
        m_mainGroup = new BGroupView(B_HORIZONTAL);
        m_mainGroup->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        m_mainGroup->GroupLayout()->SetInsets(5, 5, 5, 5);
        AddChild(m_mainGroup);

        m_okButton = new BButton("ok", "Done", new BMessage('done'));
        BButton* cancel = new BButton("cancel", "Cancel", new BMessage('canc'));

        BGroupView* bottomGroup = new BGroupView(B_HORIZONTAL);
        bottomGroup->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        BGroupLayoutBuilder(bottomGroup)
            .SetInsets(5, 5, 5, 5)
            .AddGlue()
            .Add(cancel)
            .Add(m_okButton);
        AddChild(bottomGroup);

        CenterOnScreen();
    }

    void Configure(const WebCore::DateTimeChooserParameters& params) {
        MoveTo(BRect(params.anchorRectInRootView).LeftTop());

        if (params.type == InputTypeNames::datetimelocal()
                || params.type == InputTypeNames::date()
                || params.type == InputTypeNames::week()
                || params.type == InputTypeNames::month()) {
            BDateFormat format;

            m_yearControl = new BTextControl("year", NULL, NULL,
                new BMessage('yech'));
            m_yearControl->SetModificationMessage(new BMessage('yech'));
            m_yearControl->TextView()->SetMaxBytes(6);

            BMenu* monthMenu = new BMenu("month");
            monthMenu->SetLabelFromMarked(true);

            for (int i = 1; i <= 12; i++) {
                BString out;
                format.GetMonthName(i, out);
                BMessage* message = new BMessage('moch');
                message->AddInt32("month", i);
                monthMenu->AddItem(new BMenuItem(out, message));
            }

            BGroupLayoutBuilder(m_mainGroup)
                .AddGroup(B_VERTICAL)
                    .AddGroup(B_HORIZONTAL)
                        .Add(new BMenuField(NULL, monthMenu))
                        .Add(m_yearControl)
                    .End()
                    .Add(m_calendar = new BPrivate::BCalendarView("Date"))
                .End()
            .End();

            BDate initialDate;

            if (params.type == InputTypeNames::date())
                format.SetDateFormat(B_LONG_DATE_FORMAT, "yyyy'-'MM'-'dd");
            else if (params.type == InputTypeNames::month()) {
                format.SetDateFormat(B_LONG_DATE_FORMAT, "yyyy'-'MM");
            } else if (params.type == InputTypeNames::week())
                format.SetDateFormat(B_LONG_DATE_FORMAT, "yyyy'-W'ww");

            BString currentValue(params.currentValue.utf8().data());
            format.Parse(currentValue, B_LONG_DATE_FORMAT, initialDate);

            m_calendar->SetDate(initialDate);

            BString out;
            format.SetDateFormat(B_SHORT_DATE_FORMAT, "yyyy");
            format.Format(out, initialDate, B_SHORT_DATE_FORMAT);
            m_yearControl->SetText(out.String());

            if (BMenuItem* item = monthMenu->ItemAt(initialDate.Month() - 1))
                item->SetMarked(true);

        }

        if (params.type == InputTypeNames::datetimelocal())
           m_mainGroup->AddChild(new BSeparatorView(B_VERTICAL));

        if (params.type == InputTypeNames::datetimelocal()
                || params.type == InputTypeNames::time()) {
            m_hourMenu = new BMenu("hour");
            m_hourMenu->SetLabelFromMarked(true);
            m_minuteMenu = new BMenu("minute");
            m_minuteMenu->SetLabelFromMarked(true);

            BTimeFormat format;
            BTime initialTime;
            format.SetTimeFormat(B_SHORT_TIME_FORMAT, "HH':'mm");

            BString currentValue(params.currentValue.utf8().data());
            format.Parse(currentValue, B_SHORT_TIME_FORMAT, initialTime);

            for (int i = 0; i <= 24; i++) {
                BString label;
                label << i;
                m_hourMenu->AddItem(new BMenuItem(label, NULL));
            }

            if (BMenuItem* item = m_hourMenu->ItemAt(initialTime.Hour()))
                item->SetMarked(true);

            for (int i = 0; i <= 60; i++) {
                BString label;
                label << i;
                m_minuteMenu->AddItem(new BMenuItem(label, NULL));
            }

            if (BMenuItem* item = m_minuteMenu->ItemAt(initialTime.Minute()))
                item->SetMarked(true);

            BGroupLayoutBuilder(m_mainGroup)
                .AddGroup(B_VERTICAL)
                    .AddGroup(B_HORIZONTAL)
                        .Add(new BMenuField(NULL, m_hourMenu))
                        .Add(new BMenuField(NULL, m_minuteMenu))
                        .AddGlue();
        }

        if (params.type == InputTypeNames::month()) {
            m_format = "yyyy'-'MM";
            m_calendar->Hide();
        } else if (params.type == InputTypeNames::week()) {
            m_format = "YYYY'-W'ww";
        } else if (params.type == InputTypeNames::date()) {
            m_format = "yyyy'-'MM'-'dd";
        } else if (params.type == InputTypeNames::time()) {
            m_format = "HH':'mm";
        } else {
            m_format = "yyyy'-'MM'-'dd'T'"; // datetime or datetime-local
        }
    }

    void MessageReceived(BMessage* message) override {
        switch(message->what) {
            case 'done':
            {
                BString str;
                BLanguage language("en");
                BFormattingConventions conventions("en_US");

                if (m_calendar) {
                    conventions.SetExplicitDateFormat(B_LONG_DATE_FORMAT, m_format);
                    BDateFormat formatter(language, conventions);
                    formatter.Format(str, m_calendar->Date(), B_LONG_DATE_FORMAT);
                }

                if (m_hourMenu && m_hourMenu->Superitem()) {
                    if (m_calendar) {
                        // Append time to date string if both exist (datetimelocal)
                    } else {
                        // Time only
                    }
                    BString timeStr;
                    timeStr << m_hourMenu->Superitem()->Label();
                    if (timeStr.Length() < 2) timeStr.Prepend("0");
                    timeStr << ':';
                    if (m_minuteMenu && m_minuteMenu->Superitem())
                        timeStr << m_minuteMenu->Superitem()->Label();

                    str << timeStr;
                }

                String dateString = String::fromUTF8(str.String());
                RunLoop::main().dispatch([picker = &m_picker, dateString]() {
                    picker->didChooseDate(dateString);
                });
                [[fallthrough]];
            }
            case 'canc':
                PostMessage(B_QUIT_REQUESTED);
                return;

            case 'moch':
            {
                if (m_calendar)
                    m_calendar->SetMonth(message->FindInt32("month"));
                return;
            }

            case 'yech':
            {
                char* p;
                errno = 0;
                int year = strtol(m_yearControl->Text(), &p, 10);
                if (errno == ERANGE || year > 275759 || year <= 0
                        || p == m_yearControl->Text() || *p != '\0') {
                    m_yearControl->MarkAsInvalid(true);
                    m_okButton->SetEnabled(false);
                } else {
                    m_yearControl->MarkAsInvalid(false);
                    m_okButton->SetEnabled(true);
                    if (m_calendar)
                        m_calendar->SetYear(year);
                }
                return;
            }
        }

        BWindow::MessageReceived(message);
    }

    bool QuitRequested() override {
        RunLoop::main().dispatch([picker = &m_picker]() {
            picker->didEndChooser();
        });
        return false;
    }

private:
    WebDateTimePickerHaiku& m_picker;
    BPrivate::BCalendarView* m_calendar;
    BTextControl* m_yearControl;
    BButton* m_okButton;
    BMenu* m_hourMenu;
    BMenu* m_minuteMenu;
    BString m_format;
    BGroupView* m_mainGroup;
};

Ref<WebDateTimePickerHaiku> WebDateTimePickerHaiku::create(WebPageProxy& page)
{
    return adoptRef(*new WebDateTimePickerHaiku(page));
}

WebDateTimePickerHaiku::WebDateTimePickerHaiku(WebPageProxy& page)
    : WebDateTimePicker(page)
    , m_window(nullptr)
{
}

WebDateTimePickerHaiku::~WebDateTimePickerHaiku()
{
    if (m_window) {
        m_window->Lock();
        m_window->Quit();
    }
}

void WebDateTimePickerHaiku::endPicker()
{
    if (m_window) {
        auto window = m_window;
        m_window = nullptr;
        window->Lock();
        window->Quit();
    }
    WebDateTimePicker::endPicker();
}

void WebDateTimePickerHaiku::showDateTimePicker(WebCore::DateTimeChooserParameters&& params)
{
    if (m_window)
        return;

    auto* window = new DateTimeChooserWindow(*this);
    window->Configure(params);
    window->Show();
    m_window = window;
}

void WebDateTimePickerHaiku::didChooseDate(const String& date)
{
    if (m_page)
        m_page->didChooseDate(date);
}

void WebDateTimePickerHaiku::didEndChooser()
{
    endPicker();
}

} // namespace WebKit
