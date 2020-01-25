#include "fmruby.h"
#include "fmruby_app.h"
#include "fmruby_fabgl.h"


FmrbSystemApp SystemApp;

FmrbSystemApp::FmrbSystemApp()
{
  m_state = FMRB_SYS_STATE::INIT;
  m_terminal_available = false;
}

FMRB_RCODE FmrbSystemApp::init_terminal(void)
{
  FMRB_DEBUG(FMRB_LOG::DEBUG,"start terminal_init\n");
  if(!m_terminal_available){
    fabgl_terminal_mode_init();

    m_terminal.begin(&VGAController);
    m_terminal.connectLocally();
    //Terminal.connectSerialPort(Serial,true);

    m_terminal.setBackgroundColor(Color::Black);
    m_terminal.setForegroundColor(Color::White);
    m_terminal.clear();
    m_terminal.enableCursor(true);
    //FMRB_DEBUG(FMRB_LOG::DEBUG,"terminal_init() done\n");
    m_terminal_available = true;
  }else{
    FMRB_DEBUG(FMRB_LOG::DEBUG,"terminal is already initialized\n");
  }
  return FMRB_RCODE::OK;
}

FMRB_RCODE FmrbSystemApp::close_terminal(void)
{
  FMRB_DEBUG(FMRB_LOG::DEBUG,"start close_terminal\n");
  if(m_terminal_available)
  {
    m_terminal.end();
    m_terminal_available = false;
  }else{
    //FMRB_DEBUG(FMRB_LOG::DEBUG,"terminal is already closed\n");
    return FMRB_RCODE::ERROR;
  }
  return FMRB_RCODE::OK;
}

void FmrbSystemApp::wait_key(char target)
{
  if(!m_terminal_available) return;

  while(true)
  {
    if (m_terminal.available())
    {
      char c = m_terminal.read();
      if(c == target){
        return;
      }
    }
    vTaskDelay(10);
  }
}

FMRB_RCODE FmrbSystemApp::print_system_info()
{
  if(!m_terminal_available) return FMRB_RCODE::ERROR;

  m_terminal.printf("\e[37m* Family mruby *   Ver. %s (%s)\r\n",FMRB_VERSION,FMRB_RELEASE);
  m_terminal.write ("\e[34m   Powereded by FabGL\e[32m\r\n\n");
  m_terminal.printf("\e[32mScreen Size        :\e[33m %d x %d\r\n", VGAController.getScreenWidth(), VGAController.getScreenHeight());
  m_terminal.printf("\e[32mTerminal Size      :\e[33m %d x %d\r\n", m_terminal.getColumns(), m_terminal.getRows());
  m_terminal.printf("\e[32mKeyboard           :\e[33m %s\r\n", PS2Controller.keyboard()->isKeyboardAvailable() ? "OK" : "Error");
  m_terminal.printf("\e[32mFree DMA Memory    :\e[33m %d\r\n", heap_caps_get_free_size(MALLOC_CAP_DMA));
  m_terminal.printf("\e[32mFree 32 bit Memory :\e[33m %d\r\n\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
  m_terminal.write("\e[32m >> Press Enter\r\n\n");
  m_terminal.write("\e[37m");
  return FMRB_RCODE::OK;
}

static void draw_img(uint16_t x0,uint16_t y0,uint8_t* data){
  const int header = 4;
  uint16_t width  = (data[header]) + (data[header+1]<<8);
  uint16_t height = (data[header+2]) + (data[header+3]<<8);
  FMRB_DEBUG(FMRB_LOG::DEBUG,"img:%d,%d\n",width,height);

  uint8_t* p = data+header+4;
  for(uint16_t y=0;y<height;y++){
    for(uint16_t x=0;x<width;x++){
      if(((*p)&0xC0) == 0 ){ //check alpha
        VGAController.setRawPixel(x0+x,y0+y,
          VGAController.createRawPixel(RGB222((*p)&0x03, ((*p)&0x0C) >> 2, ((*p)&0x30) >> 4)));
      }
      p++;
    }
  }
}

FMRB_RCODE FmrbSystemApp::show_splash(){
  //uint8_t* img_data = (uint8_t*)FMRB_storage.load("/test.img");
  uint32_t fsize;
  //uint8_t* img_data = (uint8_t*)FMRB_storage.load("/bktest.img",fsize,false,false);
  uint8_t* img_data = (uint8_t*)FMRB_storage.load("/bk_small.img",fsize,false,false);
  if(img_data){
    draw_img(0,0,img_data);
    fmrb_free(img_data);
  }
  print_system_info();
  return FMRB_RCODE::OK;
}


FMRB_RCODE FmrbSystemApp::run_editor(){
    m_editor.begin(&m_terminal);
    int err = m_editor.run(m_script);
    m_script = NULL;
    if(err >= 0){
      m_script = m_editor.dump_script();
    }
    m_editor.release();
  return FMRB_RCODE::OK;
}

FMRB_RCODE FmrbSystemApp::run_mruby(){
  if(m_script){
    fabgl_mruby_mode_init();
    m_mruby_engine.run(m_script);
    FMRB_DEBUG(FMRB_LOG::DEBUG,"m_mruby_engine END\n");
  }
  return FMRB_RCODE::OK;

}

FMRB_RCODE FmrbSystemApp::run()
{
  while(true){
    FMRB_DEBUG(FMRB_LOG::MSG,"[AppState:%d]\n",m_state);
    //Booting Family mruby
    switch(m_state){
      case FMRB_SYS_STATE::INIT:
      {
        init_terminal();
        m_script = NULL;
        FMRB_storage.init();
        show_splash();
        wait_key(0x0D);
        m_state = FMRB_SYS_STATE::SHOW_MENU;
        break;
      }
      case FMRB_SYS_STATE::SHOW_MENU:
      {
        if(!m_terminal_available){
          init_terminal();
        }
        m_state = FMRB_SYS_STATE::DO_EDIT;
        break;
      }
      case FMRB_SYS_STATE::DO_EDIT:
      {
        if(!m_terminal_available){
          init_terminal();
        }
        run_editor();
        m_state = FMRB_SYS_STATE::EXEC_FROM_EDIT;
        break;
      }
      case FMRB_SYS_STATE::EXEC_FROM_EDIT:
      {
        if(m_terminal_available) close_terminal();
        run_mruby();
        m_state = FMRB_SYS_STATE::DO_EDIT;
        break;
      }
      case FMRB_SYS_STATE::EXEC_FROM_FILE:
      default:
      return FMRB_RCODE::ERROR;
    }
  }
  return FMRB_RCODE::OK;
}

//#define TEST_SCRIPT
#ifndef TEST_SCRIPT
void menu_app()
{
  SystemApp.run();
}
#else
const char* sample_script2 = 
#include "./mrb/entry_mrb.rb";

void menu_app(){ //Test
  FMRB_DEBUG(FMRB_LOG::DEBUG,"ScriptTest\n");
  const char* scirpt = sample_script2;
  if(scirpt){
    fabgl_mruby_mode_init();
    mruby_engine(scirpt);
  }
}
#endif
