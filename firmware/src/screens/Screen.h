#pragma once

#include <lvgl.h>

/* Interface comum de todas as telas.
 *
 * Ciclo de vida: create() e chamado uma unica vez pelo ScreenManager no boot;
 * onShow()/onHide() a cada troca; onStateChanged() apenas na tela ativa quando
 * o AppState muda. O grupo de encoder da tela e trocado no indev pelo
 * ScreenManager a cada show. */
class Screen {
public:
    virtual ~Screen() = default;

    /* Nome exibido no menu global (pt-BR, curto). */
    virtual const char *name() const = 0;

    virtual void create() = 0;
    virtual void onShow() {}
    virtual void onHide() {}
    virtual void onStateChanged() {}

    lv_obj_t *root() const { return root_; }
    lv_group_t *group() const { return group_; }

protected:
    /* Cria a tela raiz e o grupo de encoder; chamar no inicio de create(). */
    void initRoot() {
        root_ = lv_obj_create(nullptr);
        group_ = lv_group_create();
    }

    lv_obj_t *root_ = nullptr;
    lv_group_t *group_ = nullptr;
};
