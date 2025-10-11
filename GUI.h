#ifndef __GUI_H__
#define __GUI_H__

#include <Arduino.h>
#include <TFT_eSPI.h> // Оптимизирана за ESP32
extern TFT_eSPI DSP;

#define MAX_WINDOWS 5
#define MAX_CONTROLS 10
#define C_WHITE TFT_WHITE // Дефинираме C_WHITE като TFT_WHITE

// Типове съобщения
enum MessageType
{
    WM_CLICK = 1, // Тъч събитие
    WM_KEY_CLICK, // Натиснат физически бутон
    WM_SET_FOCUS, // Промяна на фокус
    WM_DRAW       // Прерисуване
};

// Структура за съобщения
typedef struct
{
    uint32_t handle; // Уникален идентификатор на обекта
    uint32_t msg;    // Тип съобщение
    uint32_t param1; // Параметър (напр. x координата или код на бутон)
    uint32_t param2; // Параметър (напр. y координата)
} MESSAGE;

// Базов клас за обекти
class TObject
{
protected:
    int x, y, w, h;          // Позиция и размери, не се редактират динамично
    bool focus;              // Флаг за фокус
    bool visible;            // Управлява се от WindowManager
    void (*userDraw)(void);  // Потребителска функция за рисуване
    virtual void draw() = 0; // Защитен метод за рисуване

    // Базова имплементация за обработка на съобщения
    void defaultMessage(MESSAGE *m)
    {
        if (m->handle == (uint32_t)this)
        {
            switch (m->msg)
            {
            case WM_KEY_CLICK:
                if (focus && userDraw)
                    userDraw();
                break;
            case WM_SET_FOCUS:
                focus = true;
                if (visible)
                {
                    if (userDraw)
                        userDraw();
                    else
                        draw();
                }
                break;
            case WM_DRAW:
                if (visible)
                {
                    if (userDraw)
                        userDraw();
                    else
                        draw();
                }
                break;
            }
        }
    }

public:
    uint16_t bgColor; // Публичен цвят на фона
    TObject(int x, int y, int w, int h, uint16_t bgColor = C_WHITE, void (*userDraw)(void) = nullptr)
        : x(x), y(y), w(w), h(h), bgColor(bgColor), focus(false), visible(false), userDraw(userDraw) {}
    virtual ~TObject() {}
    virtual void handleMessage(MESSAGE *m) = 0; // Чисто виртуален метод
    void setFocus(bool f) { focus = f; }
    bool isFocused() { return focus; }
};

// Клас за прозорец
class TWindow : public TObject
{
protected:
    TObject *controls[MAX_CONTROLS];
    uint8_t controlCount;
    uint8_t focusedControl; // Индекс на фокусирания контрол

    void draw() override
    { // Защитен метод
        if (!visible)
            return;
        if (userDraw)
            userDraw();
        else
            DSP.fillRect(x, y, w, h, bgColor);
        for (uint8_t i = 0; i < controlCount; i++)
        {
            if (controls[i])
            {
                controls[i]->visible = true; // Контролите наследяват видимост
                controls[i]->draw();
            }
        }
    }

public:
    TWindow(int x, int y, int w, int h, uint16_t bgColor = C_WHITE, void (*userDraw)(void) = nullptr)
        : TObject(x, y, w, h, bgColor, userDraw), controlCount(0), focusedControl(0)
    {
        for (uint8_t i = 0; i < MAX_CONTROLS; i++)
            controls[i] = nullptr;
    }

    void add(TObject *control)
    {
        // Проверяваме дали контролът не е прозорец
        if (control && !dynamic_cast<TWindow *>(control) && controlCount < MAX_CONTROLS)
        {
            controls[controlCount++] = control;
        }
    }

    void handleMessage(MESSAGE *m) override
    {
        if (m->handle == (uint32_t)this)
        {
            switch (m->msg)
            {
            case WM_SET_FOCUS:
                if (m->param1 < controlCount)
                {
                    for (uint8_t i = 0; i < controlCount; i++)
                    {
                        controls[i]->setFocus(i == m->param1);
                    }
                    focusedControl = m->param1;
                    focus = false; // Прозорецът губи фокус
                    if (visible)
                    {
                        if (userDraw)
                            userDraw();
                        else
                            draw();
                    }
                }
                else
                {
                    // Задаване на фокус на прозореца
                    focus = true;
                    for (uint8_t i = 0; i < controlCount; i++)
                    {
                        if (controls[i])
                            controls[i]->setFocus(false);
                    }
                    focusedControl = 0;
                    if (visible)
                    {
                        if (userDraw)
                            userDraw();
                        else
                            draw();
                    }
                }
                break;
            case WM_CLICK:
            case WM_KEY_CLICK:
                // Ако няма фокусиран контрол, прозорецът обработва
                if (focusedControl >= controlCount || !controls[focusedControl])
                {
                    if (focus && userDraw)
                        userDraw();
                }
                else
                {
                    m->handle = (uint32_t)controls[focusedControl];
                    controls[focusedControl]->handleMessage(m);
                }
                break;
            case WM_DRAW:
                if (visible)
                {
                    if (userDraw)
                        userDraw();
                    else
                        draw();
                }
                break;
            }
        }
        else
        {
            // Препращане към съответния контрол
            for (uint8_t i = 0; i < controlCount; i++)
            {
                if (controls[i] && m->handle == (uint32_t)controls[i])
                {
                    controls[i]->handleMessage(m);
                    break;
                }
            }
        }
    }

    void setVisible(bool v)
    {
        visible = v;
        for (uint8_t i = 0; i < controlCount; i++)
        {
            if (controls[i])
                controls[i]->visible = v; // Контролите наследяват видимост
        }
        if (v)
            draw();
    }

    bool isVisible() { return visible; }

    TObject *getFocusedControl()
    {
        return (focusedControl < controlCount) ? controls[focusedControl] : nullptr;
    }
};

// Клас за управление на прозорци
class WindowManager
{
private:
    TWindow *windows[MAX_WINDOWS];
    uint8_t windowCount;
    uint8_t activeWindowIndex;

public:
    WindowManager() : windowCount(0), activeWindowIndex(0)
    {
        for (uint8_t i = 0; i < MAX_WINDOWS; i++)
            windows[i] = nullptr;
    }

    void addWindow(TWindow *window)
    {
        if (windowCount < MAX_WINDOWS)
            windows[windowCount++] = window;
    }

    void show(TWindow &window)
    {
        // Нулиране на фокуса на всички прозорци и контроли
        for (uint8_t i = 0; i < windowCount; i++)
        {
            if (windows[i])
            {
                windows[i]->setFocus(false);
                windows[i]->setVisible(false);
                for (uint8_t j = 0; j < MAX_CONTROLS; j++)
                {
                    if (windows[i]->controls[j])
                    {
                        windows[i]->controls[j]->setFocus(false);
                    }
                }
            }
        }
        // Намиране на прозореца в масива
        for (uint8_t i = 0; i < windowCount; i++)
        {
            if (windows[i] == &window)
            {
                activeWindowIndex = i;
                windows[i]->setVisible(true);
                windows[i]->setFocus(true); // Прозорецът получава фокус
                break;
            }
        }
    }

    void dispatchMessage(MESSAGE *m)
    {
        if (windows[activeWindowIndex] && windows[activeWindowIndex]->isVisible())
        {
            windows[activeWindowIndex]->handleMessage(m);
        }
    }

    TWindow *getActiveWindow()
    {
        return (activeWindowIndex < windowCount) ? windows[activeWindowIndex] : nullptr;
    }
};

// Пример за бутон (сложен контрол)
class Button : public TObject
{
protected:
    void draw() override
    { // Защитен метод
        if (!visible)
            return;
        if (userDraw)
            userDraw();
        else
        {
            DSP.fillRect(x, y, w, h, bgColor);
            DSP.setTextColor(focus ? TFT_RED : TFT_BLACK);
            DSP.setCursor(x + 5, y + 5);
            DSP.print(text);
        }
    }

public:
    Button(int x, int y, int w, int h, uint16_t bgColor, const char *text)
        : TObject(x, y, w, h, bgColor), text(text) {}

    void handleMessage(MESSAGE *m) override
    {
        if (m->handle == (uint32_t)this)
        {
            switch (m->msg)
            {
            case WM_CLICK:
                // При тъч събитие винаги извикваме userDraw, ако има
                if (userDraw)
                    userDraw();
                break;
            default:
                defaultMessage(m); // Използваме базовата имплементация за останалите съобщения
                break;
            }
        }
    }

private:
    const char *text;
};

// Пример за Label (прост контрол)
class Label : public TObject
{
protected:
    void draw() override
    {
        if (!visible)
            return;
        if (userDraw)
            userDraw();
        else
        {
            DSP.fillRect(x, y, w, h, bgColor);
            DSP.setTextColor(focus ? TFT_RED : TFT_BLACK);
            DSP.setCursor(x + 5, y + 5);
            DSP.print(text);
        }
    }

public:
    Label(int x, int y, int w, int h, uint16_t bgColor, const char *text)
        : TObject(x, y, w, h, bgColor), text(text) {}

    void handleMessage(MESSAGE *m) override
    {
        defaultMessage(m); // Простият контрол използва базовата имплементация
    }

private:
    const char *text;
};

// Функция за събиране на съобщения
bool GetMessage(MESSAGE *msg, WindowManager &wm)
{
    // Тъч събития
    uint16_t x, y;
    if (DSP.getTouch(&x, &y))
    {
        TWindow *activeWindow = wm.getActiveWindow();
        if (activeWindow && activeWindow->isVisible())
        {
            // Проверка за докосване на контрол
            for (uint8_t i = 0; i < MAX_CONTROLS; i++)
            {
                if (activeWindow->controls[i] &&
                    x >= activeWindow->controls[i]->x && x <= activeWindow->controls[i]->x + activeWindow->controls[i]->w &&
                    y >= activeWindow->controls[i]->y && y <= activeWindow->controls[i]->y + activeWindow->controls[i]->h)
                {
                    msg->handle = (uint32_t)activeWindow->controls[i];
                    msg->msg = WM_CLICK;
                    msg->param1 = x;
                    msg->param2 = y;
                    // Задаване на фокус на контрола
                    MESSAGE focusMsg = {(uint32_t)activeWindow, WM_SET_FOCUS, i, 0};
                    activeWindow->handleMessage(&focusMsg);
                    return true;
                }
            }
            // Ако няма докоснат контрол, съобщението отива към прозореца
            msg->handle = (uint32_t)activeWindow;
            msg->msg = WM_CLICK;
            msg->param1 = x;
            msg->param2 = y;
            return true;
        }
    }

    // Физически бутон (напр. пин 4)
    static uint8_t lastButtonState = HIGH;
    uint8_t buttonState = digitalRead(4);
    if (buttonState != lastButtonState && buttonState == LOW)
    {
        TWindow *activeWindow = wm.getActiveWindow();
        if (activeWindow && activeWindow->isVisible())
        {
            TObject *focusedControl = activeWindow->getFocusedControl();
            msg->handle = (uint32_t)(focusedControl ? focusedControl : activeWindow);
            msg->msg = WM_KEY_CLICK;
            msg->param1 = 1; // Код на бутона
            msg->param2 = 0;
            lastButtonState = buttonState;
            return true;
        }
    }
    lastButtonState = buttonState;

    return false; // Няма съобщение
}

#endif