#pragma once

#include "Screen.h"

/* Menu global: lista gerada automaticamente do registro do ScreenManager. */
class MenuScreen : public Screen {
public:
    const char *name() const override { return "menu"; }
    void create() override;
    void onShow() override;

private:
    lv_obj_t *list_ = nullptr;
};
