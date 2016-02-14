//FractalServer - web server
#ifndef FractalServer__
#define FractalServer__

#ifndef DISABLE_SERVER

#include "OutputInterface.h"
#include "OpenGLViewer.h"
#include "mongoose/mongoose.h"

class FractalServer : public OutputInterface
{
  //Singleton class, construct with FractalServer::Instance()
protected:
  //FractalServer() {}   //Protected constructors
  FractalServer(OpenGLViewer* viewer, std::string htmlpath, int port, int quality, int threads);
  //FractalServer(FractalServer const&) {} // copy constructor is protected
  //FractalServer& operator=(FractalServer const&) {} // assignment operator is protected

  static FractalServer* _self;

  OpenGLViewer* viewer;

  int port, threads;
  std::string path;

  // Thread sync
  pthread_mutex_t cs_mutex;
  pthread_cond_t condition_var;
  std::deque<std::string> commands;

  int clients;
  int wait_count;
  int client_id;
  int quality;
  bool updated;
  std::map<int,bool> synched; //Client status
  GLubyte *imageCache;

public:
  bool imageserver;
  unsigned char* jpeg;
  int jpeg_bytes;

   // Display Thread sync
   pthread_mutex_t p_mutex;
   static pthread_cond_t p_condition_var;

   static std::deque<std::string> data;
   static bool images;
   static bool resized;

   GLubyte *image;
   std::string encoded;

   void render();

  //Public instance constructor/getter
  static FractalServer* Instance(OpenGLViewer* viewer, std::string htmlpath, int port, int quality=90, int threads=1);
  static FractalServer* Instance() {return _self;}

  static void Delete()
  {
    if (_self) delete _self;
    _self = NULL;
  }
  virtual ~FractalServer();
  struct mg_context* ctx;

  void send_image(struct mg_connection *conn);

  static int request(struct mg_connection *conn);

  // virtual functions for window management
  virtual void open(int width, int height);
  virtual void resize(int new_width, int new_height);
  virtual void display();
  virtual void close();
  virtual void idle() {}

  bool compare(GLubyte* image);
};

#endif //DISABLE_SERVER
#endif //FractalServer__
