#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define BTN_UP    D5
#define BTN_DOWN  D6
#define BTN_ENTER D7

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

ESP8266WebServer server(80);

/* ---------- States ---------- */
enum AppState { MAIN_MENU, FILE_LIST, FILE_VIEW, UPLOAD_MODE };
AppState state = MAIN_MENU;

/* ---------- Button Events ---------- */
enum BtnEvent { NONE, UP, DOWN, ENTER };
BtnEvent btnEvent = NONE;
bool lastU=1,lastD=1,lastE=1;

/* ---------- Menu ---------- */
int mainMenuIndex = 0;

/* ---------- Files ---------- */
String files[20];
int fileCount = 0;
int fileIndex = 0;
int viewLine = 0;
String fileText;

/* ---------- Button Read ---------- */
void readButtons(){
  bool u=digitalRead(BTN_UP);
  bool d=digitalRead(BTN_DOWN);
  bool e=digitalRead(BTN_ENTER);
  btnEvent = NONE;
  if(lastU && !u) btnEvent=UP;
  if(lastD && !d) btnEvent=DOWN;
  if(lastE && !e) btnEvent=ENTER;
  lastU=u; lastD=d; lastE=e;
}

/* ---------- FS ---------- */
void loadFileList(){
  fileCount=0;
  Dir dir = LittleFS.openDir("/");
  while(dir.next() && fileCount<20){
    files[fileCount++] = dir.fileName();
  }
  fileIndex=0;
}

/* ---------- Draw ---------- */
void drawMainMenu(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(mainMenuIndex==0?"> Start":"  Start");
  display.println(mainMenuIndex==1?"> Upload":"  Upload");
  display.display();
}

void drawFileList(){
  display.clearDisplay();
  for(int i=0;i<3;i++){
    int idx = fileIndex+i;
    if(idx<fileCount){
      display.setCursor(0,i*10);
      display.print(i==0?"> ":"  ");
      display.println(files[idx]);
    }
  }
  display.display();
}

void drawFileView(){
  display.clearDisplay();
  for(int i=0;i<3;i++){
    int start = (viewLine+i)*16;
    if(start < fileText.length()){
      display.setCursor(0,i*10);
      display.println(fileText.substring(start,start+16));
    }
  }
  display.display();
}

void drawUpload(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("UPLOAD MODE");
  display.println(WiFi.softAPIP());
  display.println("Enter=Exit");
  display.display();
}

/* ---------- Web Server ---------- */
File uploadFile;

void startServer(){
  WiFi.softAP("ESP_Upload","12345678");

  server.on("/",HTTP_GET,[](){
    String page="<h2>Files</h2>";
    Dir dir = LittleFS.openDir("/");
    while(dir.next()){
      page+=dir.fileName();
      page+=" <a href='/del?f="+dir.fileName()+"'>[DEL]</a><br>";
    }
    page+="<form method='POST' action='/upload' enctype='multipart/form-data'>"
          "<input type='file' name='f'><input type='submit'></form>";
    server.send(200,"text/html",page);
  });

  server.on("/del",HTTP_GET,[](){
    if(server.hasArg("f")) LittleFS.remove(server.arg("f"));
    server.sendHeader("Location","/");
    server.send(303);
  });

  server.on("/upload",HTTP_POST,[](){
    server.send(200,"text/plain","OK");
  },[](){
    HTTPUpload& up = server.upload();
    if(up.status==UPLOAD_FILE_START){
      uploadFile = LittleFS.open("/"+up.filename,"w");
    }else if(up.status==UPLOAD_FILE_WRITE){
      uploadFile.write(up.buf, up.currentSize);
    }else if(up.status==UPLOAD_FILE_END){
      uploadFile.close();
    }
  });

  server.begin();
}

void stopServer(){
  server.stop();
  WiFi.softAPdisconnect(true);
}

/* ---------- State Handlers ---------- */
void handleMainMenu(){
  if(btnEvent==UP && mainMenuIndex>0) mainMenuIndex--;
  if(btnEvent==DOWN && mainMenuIndex<1) mainMenuIndex++;
  if(btnEvent==ENTER){
    if(mainMenuIndex==0){
      loadFileList();
      state=FILE_LIST;
    }else{
      startServer();
      state=UPLOAD_MODE;
    }
  }
  drawMainMenu();
}

void handleFileList(){
  if(btnEvent==UP && fileIndex>0) fileIndex--;
  if(btnEvent==DOWN && fileIndex<fileCount-1) fileIndex++;
  if(btnEvent==ENTER && fileCount>0){
    File f = LittleFS.open(files[fileIndex],"r");
    fileText = f.readString();
    f.close();
    viewLine=0;
    state=FILE_VIEW;
  }
  drawFileList();
}

void handleFileView(){
  int maxLines = fileText.length()/16;
  if(btnEvent==UP && viewLine>0) viewLine--;
  if(btnEvent==DOWN && viewLine<maxLines) viewLine++;
  if(btnEvent==ENTER) state=FILE_LIST;
  drawFileView();
}

void handleUpload(){
  server.handleClient();
  if(btnEvent==ENTER){
    stopServer();
    state=MAIN_MENU;
  }
  drawUpload();
}

/* ---------- Setup & Loop ---------- */
void setup(){
  Serial.begin(115200);
  pinMode(BTN_UP,INPUT_PULLUP);
  pinMode(BTN_DOWN,INPUT_PULLUP);
  pinMode(BTN_ENTER,INPUT_PULLUP);

  display.begin(SSD1306_SWITCHCAPVCC,0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();

  LittleFS.begin();
}

void loop(){
  readButtons();
  switch(state){
    case MAIN_MENU: handleMainMenu(); break;
    case FILE_LIST: handleFileList(); break;
    case FILE_VIEW: handleFileView(); break;
    case UPLOAD_MODE: handleUpload(); break;
  }
  yield();
}
