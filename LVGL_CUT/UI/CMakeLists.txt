SET(SOURCES screens/ui_ScreenMain.c
    screens/ui_ScreenSetting.c
    screens/ui_ScreenWifi.c
    screens/ui_ScreenAlarm.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img______16_png.c
    images/ui_img_3995370755237016655.c
    images/ui_img_10733332317258350221.c
    images/ui_img_10069706176981150712.c
    images/ui_img_16426276093224008068.c
    images/ui_img_8235756044252224914.c
    images/ui_img_3124159366164166490.c
    images/ui_img_17190989648416166707.c
    images/ui_img_wifi___png.c
    images/ui_img_19311024801095951.c
    images/ui_img_13220784170299957105.c
    fonts/ui_font_KaiTi.c
    fonts/ui_font_KaiTi12.c
    fonts/ui_font_KaiTi14.c
    fonts/ui_font_KaiTi18.c
    fonts/ui_font_KaiTi22.c)

add_library(ui ${SOURCES})
