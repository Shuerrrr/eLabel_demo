// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.4.2
// LVGL version: 8.3.6
// Project name: SquareLine_Project

#include "../ui.h"

void ui_Screen1_screen_init(void)
{
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Roller1 = lv_roller_create(ui_Screen1);
    lv_roller_set_options(ui_Roller1, "Option 1\nOption 2\nOption 3", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(ui_Roller1, 1, LV_ANIM_OFF);
    lv_obj_set_height(ui_Roller1, 188);
    lv_obj_set_width(ui_Roller1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_x(ui_Roller1, 3);
    lv_obj_set_y(ui_Roller1, 6);
    lv_obj_set_align(ui_Roller1, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Roller1, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_set_style_text_font(ui_Roller1, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_font(ui_Roller1, &lv_font_montserrat_20, LV_PART_SELECTED | LV_STATE_DEFAULT);

    ui_Label8 = lv_label_create(ui_Screen1);
    lv_obj_set_width(ui_Label8, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label8, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Label8, 2);
    lv_obj_set_y(ui_Label8, -67);
    lv_obj_set_align(ui_Label8, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label8, "Tasks List");
    lv_obj_add_flag(ui_Label8, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_set_style_text_font(ui_Label8, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label6 = lv_label_create(ui_Screen1);
    lv_obj_set_width(ui_Label6, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label6, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Label6, 2);
    lv_obj_set_y(ui_Label6, 4);
    lv_obj_set_align(ui_Label6, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label6, "Nothing left yet\n");
    lv_obj_set_style_text_font(ui_Label6, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label12 = lv_label_create(ui_Screen1);
    lv_obj_set_width(ui_Label12, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label12, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Label12, 3);
    lv_obj_set_y(ui_Label12, 15);
    lv_obj_set_align(ui_Label12, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label12, "Enjoy your time");
    lv_obj_set_style_text_font(ui_Label12, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_Screen1, ui_event_Screen1, LV_EVENT_ALL, NULL);

}
