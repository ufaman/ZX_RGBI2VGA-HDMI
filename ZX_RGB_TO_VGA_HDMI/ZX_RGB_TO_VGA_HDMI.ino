#include <Arduino.h>
#include <typeinfo>


extern "C"{

    #include "pico/stdlib.h"
    #include "hardware/vreg.h"
    #include "hardware/flash.h"


    #include "rgb_capture.h"
    #include "stdio.h"
    #include "v_buf.h"
    #include "VGA.h"
    #include "HDMI.h"
}

#define printf Serial.printf

cap_set_t capture_setings;

static void draw_hello_image()
{
    uint8_t* vbuf=(uint8_t*)v_buf_get_out();    // Адрес массива экранной памяти размером V_BUF_H*V_BUF_W/2 (2 точки на байт)

    for(int y=0; y<V_BUF_H; y++)
        for(int x=0; x<V_BUF_W/2; x++)
        {
            // Поменяв ниже y на x, получим вертикальные полосы
            uint8_t i=y&0x0f;             // Делим экран на 16 горизонтальных полос
            uint8_t c=((i&1)<<3)|(i>>1);  // Последовательность 0, 8, 1, 9, 2, a, 3, b , 4, c, 5, d, 6, e, 7, f   : b3 - яркость, b2-b0 - цвет
            c|=c<<4;                      // В байте 2 точки. Копируем.
            *vbuf++ = c;
        }
}

void drawPixel(short x, short y, char color, char bright=0) {
    if (x>V_BUF_H-1) x=V_BUF_H-1 ;
    if (x<0) x=0;
    if (y<0) y=0;
    if (y > V_BUF_W-1) y = V_BUF_W-1;

    int pixel = ((V_BUF_H * y) + x) ;
    if (pixel & 1) g_gbuf[pixel>>1] = (g_gbuf[pixel>>1] & 0x0F) | ((bright<<3 | color) << 4);
        else       g_gbuf[pixel>>1] = (g_gbuf[pixel>>1] & 0xF0) |  (bright<<3 | color);
}

void drawVLine(short x, short y, short h, char color, char bright=0) {
    for (short i=y; i<(y+h); i++) {
        drawPixel(x, i, color, bright) ;
    }
}

void drawHLine(short x, short y, short w, char color, char bright=0) {
    for (short i=x; i<(x+w); i++) {
        drawPixel(i, y, color, bright) ;
    }
}

void drawRect(short x, short y, short w, short h, char color, char bright=0) {
    drawHLine(x, y, w, color, bright);
    drawHLine(x, y+h-1, w, color, bright);
    drawVLine(x, y, h, color, bright);
    drawVLine(x+w-1, y, h, color, bright);
}

// int data_for_save[FLASH_PAGE_SIZE/sizeof(int)]; 
const int *flash_data_for_save = (const int *) (XIP_BASE + (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE));

bool is_start_core0=false;

void setup() {
    // Для VGA эта часть точно не нужна.  Частота пикселов для VGA 640*480 25,175 МГц
    vreg_set_voltage(VREG_VOLTAGE_1_25);    // Поднимаем напряжение питания для стабильности работы при разгоне до 252 МГц (Номинал 125 МГц - разгон в 2 раза)
    sleep_ms(100);
    set_sys_clock_khz(252000, true);        // Разгон до 252 МГц.
    sleep_ms(10);

    Serial.begin(115200);

//      //загружаем ранее сохранённые данные заxвата
     memcpy(&capture_setings,flash_data_for_save,sizeof(cap_set_t));
   
//    // memset(&capture_setings,0,sizeof(cap_set_t));//test
//     //подправляем , если в ячйках мусор
  check_cap_data(&capture_setings);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // если попадём в настройки после wdt_reboot, то индикатор это покажет

 

    if (watchdog_caused_reboot())
        { 

            char s_key[20];
            int s_data;

            bool is_save=false;
            //цикл с разбором команд
            while(1)
            {
                // sleep_ms(1);
                

                //int i= scanf("%19s",s_key);
                String s1=Serial.readStringUntil('\n');
                if (s1.length()==0) continue;
                sscanf(s1.c_str(),"%19s%d",s_key,&s_data);


                if (strcmp(s_key, "ping")==0) { printf("ping ok\n"); continue;};
                if (strcmp(s_key, "mode")==0) { printf("mode 0\n"); continue;};

                if (strcmp(s_key, "exit")==0) { printf("exit ok\n"); break;};
                if (strcmp(s_key, "save")==0) { is_save=true; printf("saving...\n"); break;};
               
             
//обработчик загрузки параметров захвата
#define CAP_SET_LOAD(x,T) {if(s_key[0]=='r' || s_key[0]=='w') {  if(s_key[0]=='w') (x)=static_cast<T>(s_data); check_cap_data(&capture_setings); printf("%s %d\n",s_key,(x)); continue;};};
                //смещение изображения
                if (strcmp(s_key+1, "cap_sh_x")==0) CAP_SET_LOAD(capture_setings.shX,int)
                if (strcmp(s_key+1, "cap_sh_y")==0) CAP_SET_LOAD(capture_setings.shY,int)
                if (strcmp(s_key+1, "cap_delay")==0) CAP_SET_LOAD(capture_setings.capture_delay,int)
                if (strcmp(s_key+1, "cap_delay_fall")==0) CAP_SET_LOAD(capture_setings.capture_delay_fall,int)
                if (strcmp(s_key+1, "cap_delay_rise")==0) CAP_SET_LOAD(capture_setings.capture_delay_rise,int)
                if (strcmp(s_key+1, "cap_ext_f_div")==0) CAP_SET_LOAD(capture_setings.ext_freq_div,int)
                if (strcmp(s_key+1, "cap_sync_mode")==0) CAP_SET_LOAD(capture_setings.in_sync_mode,in_sync_mode_t)
                if (strcmp(s_key+1, "cap_len_VS")==0) CAP_SET_LOAD(capture_setings.len_VS,int)
                if (strcmp(s_key+1, "cap_p_clk_mode")==0) CAP_SET_LOAD(capture_setings.p_clk_mode,p_clk_mode_t)
                if (strcmp(s_key+1, "video_out")==0) CAP_SET_LOAD(capture_setings.video_out_mode,video_out_mode_t)
                if (strcmp(s_key+1, "cap_int_f")==0) CAP_SET_LOAD(capture_setings.int_freq,int)
                if (strcmp(s_key+1, "cap_in_inv_mask")==0) CAP_SET_LOAD(capture_setings.inv_capture_pin_mask,int)
                if (strcmp(s_key+1, "is_3X_bufmode")==0) CAP_SET_LOAD(capture_setings.is_3X_Buf,int)
                if (strcmp(s_key+1, "c_mode")==0) CAP_SET_LOAD(capture_setings.c_mode,c_mode_t)
                if (strcmp(s_key+1, "wide_mode")==0) CAP_SET_LOAD(capture_setings.is_wide_mode,int)


                printf("wrong command\n");

            }
            //сохранение параметров
            if (is_save)
            {
                check_cap_data(&capture_setings);
                uint32_t ints = save_and_disable_interrupts();
                flash_range_erase((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);                           
                flash_range_program((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), ( uint8_t*) &capture_setings, FLASH_PAGE_SIZE);
                restore_interrupts (ints);

                printf("saving data\n");
            };

        }   

  if (capture_setings.is_3X_Buf) enable_3x_bufmode(); else disable_3x_bufmode();


  digitalWrite(LED_BUILTIN, LOW);  // сбрасываем индикаторный светодиод
  draw_hello_image(); 



  if (capture_setings.video_out_mode==VGA) {startVGA();}//setVGAWideMode(capture_setings.is_wide_mode);
  if (capture_setings.video_out_mode==HDMI) { startHDMI();};
  

  is_start_core0=true;

}

// the loop function runs over and over again forever

void loop() {
    char s_key[20];
    int s_data;
    sleep_ms(3);


    String s1=Serial.readStringUntil('\n');
    if (s1.length()==0) return;
    sscanf(s1.c_str(),"%19s%d",s_key,&s_data);
    
    if (strcmp(s_key, "ping")==0) { printf("ping ok\n"); return;};
    if (strcmp(s_key, "reset")==0 || strcmp(s_key, "restart")==0 ) {printf("reset...\n");rp2040.restart();};
    if (strcmp(s_key, "mode")==0) { printf("mode 1\n"); return;};

        

        
    if (strcmp(s_key+1, "cap_sh_x")==0) {if(s_key[0]=='w') set_cap_shx(s_data); return;}
    if (strcmp(s_key+1, "cap_sh_y")==0) {if(s_key[0]=='w') set_cap_shy(s_data); return;}

   

}

void setup1()
{
    while(!is_start_core0)   sleep_ms(3);


        startCapture(&capture_setings);

}


void loop1() {
    sleep_ms(1000);    
   
}
