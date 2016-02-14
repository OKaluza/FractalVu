#ifndef DISABLE_SERVER
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "FractalServer.h"
#include "Util.h"
#if defined _WIN32
#include <SDL/SDL_syswm.h>
#endif

#include "base64.h"
#include "jpeg/jpge.h"

FractalServer* FractalServer::_self = NULL; //Static
std::deque<std::string> FractalServer::data;
pthread_cond_t FractalServer::p_condition_var;
bool FractalServer::images = false;
bool FractalServer::resized = false;

FractalServer* FractalServer::Instance(OpenGLViewer* viewer, std::string htmlpath, int port, int quality, int threads)
{
  if (!_self)   // Only allow one instance of class to be generated.
    _self = new FractalServer(viewer, htmlpath, port, quality, threads);

  return _self;
}

FractalServer::FractalServer(OpenGLViewer* viewer, std::string htmlpath, int port, int quality, int threads)
  : viewer(viewer), port(port), threads(threads), path(htmlpath), quality(quality)
{
  imageserver = false;
  imageCache = NULL;
  image = NULL;
  jpeg = NULL;
  updated = false;
  client_id = 0;
  clients = 0;
  // Initialize mutex and condition variable objects
  pthread_mutex_init(&cs_mutex, NULL);
  pthread_mutex_init(&viewer->cmd_mutex, NULL);
  pthread_cond_init (&condition_var, NULL);
  pthread_mutex_init(&p_mutex, NULL);
  pthread_cond_init (&p_condition_var, NULL);
}

FractalServer::~FractalServer()
{
  pthread_cond_broadcast(&condition_var);  //Display complete signal
  if (ctx)
    mg_stop(ctx);
}

// virtual functions for window management
void FractalServer::open(int width, int height)
{
  //Enable the animation timer
  //viewer->animate(250);   //1/4 sec timer
  struct mg_callbacks callbacks;

  char ports[16], threadstr[16];
  sprintf(ports, "%d", port);
  sprintf(threadstr, "%d", threads);
  debug_print("html path: %s ports: %s\n", path.c_str(), ports);
  const char *options[] =
  {
    "document_root", path.c_str(),
    "listening_ports", ports,
    "num_threads", threadstr,
    NULL
  };

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.begin_request = &FractalServer::request;
  if ((ctx = mg_start(&callbacks, NULL, options)) == NULL)
    abort_program("%s\n", "Cannot start http server, fatal exit");
}

void FractalServer::resize(int new_width, int new_height)
{
  printf("RESIZE DONE\n");
  FractalServer::resized = false; //Unflag resize pending
         viewer->postdisplay = true;
}

bool FractalServer::compare(GLubyte* image)
{
  bool match = false;
  if (imageCache)
  {
    match = true;
    size_t size = viewer->width * viewer->height * 3;
    for (unsigned int i=0; i<size; i++)
    {
      if (image[i] != imageCache[i])
      {
        match = false;
        break;
      }
    }
    delete[] imageCache;
  }
  imageCache = image;
  return match;
}

void FractalServer::display()
{
  //This is slow, so skip when not actually sending pixels to the remote controller
  if (!FractalServer::images || FractalServer::resized) return; 

  //Image serving can be disabled by setting quality to 0
  if (quality == 0) return;

  //If not currently sending an image, update the image data
  if (pthread_mutex_trylock(&cs_mutex) == 0)
  {
    //CRITICAL SECTION
    if (image) delete[] image;
    size_t size = viewer->width * viewer->height * 3;
    image = new GLubyte[size];
    // Read the pixels (flipped)
    viewer->pixels(image, false, true);

      updated = true; //Set new frame rendered flag
         encoded.clear();   //Delete cached data
         pthread_cond_signal(&condition_var);  //Display complete signal
         //   pthread_cond_broadcast(&condition_var);  //Display complete signal
    pthread_mutex_unlock(&cs_mutex); //END CRITICAL SECTION
  }
  else
  {
    viewer->postdisplay = true;  //Need to update
    debug_print("DELAYING IMAGE UPDATE\n");
  }

   //Clear this or leave on?
   FractalServer::images = false; //Flag image request fullfilled
}

void FractalServer::render()
{
   pthread_mutex_lock(&cs_mutex);

   //CRITICAL SECTION
   if (image) delete[] image;
   size_t size = viewer->width * viewer->height * 3;
   image = new GLubyte[size];
   // Read the pixels (flipped)
   viewer->pixels(image, false, true);

   updated = true; //Set new frame rendered flag
   encoded.clear();   //Delete cached data
   debug_print("DISPLAY: SIGNAL IMAGE UPDATE\n");
   pthread_cond_signal(&condition_var);  //Display complete signal
   //   pthread_cond_broadcast(&condition_var);  //Display complete signal
   pthread_mutex_unlock(&cs_mutex); //END CRITICAL SECTION
}
         

void FractalServer::close()
{
}

void send_file(const char *fname, struct mg_connection *conn)
{
  std::ifstream file(fname, std::ios::binary);
  std::streambuf* raw_buffer = file.rdbuf();

  //size_t len = raw_buffer->pubseekoff(0, std::ios::end, std::ios::in);;
  file.seekg(0,std::ios::end);
  size_t len = file.tellg();
  file.seekg(0,std::ios::beg);

  char* src = new char[len];
  raw_buffer->sgetn(src, len);

  //Base64 encode!
  std::string encoded = base64_encode(reinterpret_cast<const unsigned char*>(src), len);
  mg_write(conn, encoded.c_str(), encoded.length());

  delete[] src;
}

void send_string(std::string str, struct mg_connection *conn)
{
  //Base64 encode!
  std::string encoded = base64_encode(reinterpret_cast<const unsigned char*>(str.c_str()), str.length());
  mg_write(conn, encoded.c_str(), encoded.length());
}

void FractalServer::send_image(struct mg_connection *conn)
{
   if (encoded.empty())
   {
      // Writes JPEG image to memory buffer. 
      // On entry, buf_size is the size of the output buffer pointed at by pBuf, which should be at least ~1024 bytes. 
      // If return value is true, buf_size will be set to the size of the compressed data.
      int buf_size = viewer->width * viewer->height * 3;
      unsigned char* pBuf = new unsigned char[buf_size];

      // Fill in the compression parameter structure.
      jpge::params params;
      params.m_quality = quality;
      params.m_subsampling = jpge::H1V1;   //H2V2/H2V1/H1V1-none/0-grayscale

      if (compress_image_to_jpeg_file_in_memory(pBuf, buf_size, viewer->width, viewer->height, 3, 
                                                (const unsigned char *)image, params))
      {
         debug_print("JPEG compressed, %dx%d size %d\n", viewer->width, viewer->height, buf_size);
         //encoded = base64_encode(reinterpret_cast<const unsigned char*>(pBuf), buf_size);

         mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\n");
         mg_printf(conn, "Content-Length: %d\r\n", buf_size);
         //Allow cross-origin requests
         mg_printf(conn, "Access-Control-Allow-Origin: *\r\n\r\n");
         //Write raw
         mg_write(conn, pBuf, buf_size);
      }
      else
         debug_print("JPEG compress error\n");

      delete[] pBuf;
   }

   /*if (!encoded.empty())
   {
      //debug_print(encoded.c_str());
      //mg_printf(conn, "parseImage('data:image/jpeg;base64,");
      mg_printf(conn, "data:image/jpeg;base64,");
      //send_string((const char*)pBuf, conn);
      //send_string(std::string((const char*)pBuf), conn);
      mg_write(conn, encoded.c_str(), encoded.length());
      //mg_printf(conn, "');");
   }*/
}

int FractalServer::request(struct mg_connection *conn)
{
  const struct mg_request_info *request_info = mg_get_request_info(conn);

  debug_print("SERVER REQUEST: %s\n", request_info->uri);

  if (strcmp("/", request_info->uri) == 0)
  {
    mg_printf(conn, "HTTP/1.1 302 Found\r\n"
              "Set-Cookie: original_url=%s\r\n"
          "Location: %s%s\r\n\r\n",
          request_info->uri, "/index.html", (_self->imageserver ? "?server=render" : "?server=control"));
  }
  else if (strstr(request_info->uri, "/clear") != NULL)
  {
    //Clear queued
    mg_printf(conn, "HTTP/1.1 200 OK\r\n\r\n");
    pthread_mutex_lock(&_self->cs_mutex);
    FractalServer::data.clear();
    pthread_mutex_unlock(&_self->cs_mutex);
  }
  else if (strstr(request_info->uri, "/update") != NULL)
  {
    //Update shader/params only, no image returned
    mg_printf(conn, "HTTP/1.1 200 OK\r\n\r\n");
    char post_data[32000];
    int post_data_len = mg_read(conn, post_data, sizeof(post_data));
    //printf("RECV %d\n", post_data_len);
    if (post_data_len)
    {
      pthread_mutex_lock(&_self->cs_mutex);
      //Push command onto queue to be processed in the viewer thread
      post_data[post_data_len-1] = '\0';
      FractalServer::data.push_back(post_data);
      _self->viewer->postdisplay = true;
      //if (_self->imageserver) pthread_cond_signal(&p_condition_var); //Required if window hidden (no timer)
         pthread_cond_signal(&p_condition_var); //Required if window hidden (no timer)
      pthread_mutex_unlock(&_self->cs_mutex);
    }

  }
  else if (strstr(request_info->uri, "/image") != NULL)
  {
    FractalServer::images = true; //Flag image requested
    //TEST - gets currently cached image only
    if (_self->image)
    {
      pthread_mutex_lock(&_self->cs_mutex);
      _self->send_image(conn);
      //Signal ready if viewer not visible (no timer active)
      pthread_cond_signal(&p_condition_var);
      pthread_mutex_unlock(&_self->cs_mutex);
    }
    else
    {
      printf("NO IMAGE BUFFERED!\n");
    }
  }
  else if (strstr(request_info->uri, "/command=") != NULL)
  {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
    std::string data = request_info->uri+1;
    //Replace commas? Allows multiple comma separated commands? TODO: check where used, switch with semicolon
    //std::replace(data.begin(), data.end(), ',', '\n');
    const size_t equals = data.find('=');
    pthread_mutex_lock(&_self->cs_mutex);
    //const size_t amp = data.find('&');
    if (std::string::npos != equals)// && std::string::npos != amp)
    {
      OpenGLViewer::commands.push_back(data.substr(equals+1));
      _self->viewer->postdisplay = true;
    }
    pthread_mutex_unlock(&_self->cs_mutex);
  }
  else if (strstr(request_info->uri, "/post") != NULL)
  {
    FractalServer::images = true; //Flag image requested
    //mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
    //mg_printf(conn, "HTTP/1.1 200 OK\r\n");
    char post_data[32000];
    int post_data_len = mg_read(conn, post_data, sizeof(post_data));
    //printf("%d\n%s\n", post_data_len, post_data);
    printf("RECV %d\n", post_data_len);
    if (post_data_len)
    {
      pthread_mutex_lock(&_self->cs_mutex);
      debug_print("%u Mutex locked\n", pthread_self());
      //Push command onto queue to be processed in the viewer thread
      //OpenGLViewer::commands.push_back(base64_decode(post_data));
      post_data[post_data_len-1] = '\0';
      OpenGLViewer::commands.push_back(post_data);
      //OpenGLViewer::commands.push_back(base64_decode(post_data));
      _self->viewer->postdisplay = true;
      pthread_t tid;
      tid = pthread_self();
       
      //Signal ready if viewer not visible (no timer active)
      pthread_cond_signal(&p_condition_var);
     
      debug_print("CLIENT THREAD ID %u UPDATED? %d\n", tid, _self->updated);
      while (!_self->updated && !_self->viewer->quitProgram)
      {
        debug_print("CLIENT THREAD ID %u WAITING\n", tid);
        pthread_cond_wait(&_self->condition_var, &_self->cs_mutex);
      }
      debug_print("CLIENT THREAD ID %u RESUMED, quit? %d\n", tid, _self->viewer->quitProgram);

      if (!_self->viewer->quitProgram)
      {
        //mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/x-javascript\r\n\r\n");
        _self->send_image(conn);
      }
      _self->updated = false;
      pthread_mutex_unlock(&_self->cs_mutex);
      debug_print("%u Mutex unlocked\n---------------------------\n", pthread_self());
    }
  }
  else if (strstr(request_info->uri, "/key=") != NULL)
  {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
    std::string data = request_info->uri+1;
    pthread_mutex_lock(&_self->viewer->cmd_mutex);
    OpenGLViewer::commands.push_back("key " + data);
    _self->viewer->postdisplay = true;
    pthread_mutex_unlock(&_self->viewer->cmd_mutex);
  }
  else if (strstr(request_info->uri, "/mouse=") != NULL)
  {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n"); //Empty response, prevent XML errors
    std::string data = request_info->uri+1;
    pthread_mutex_lock(&_self->viewer->cmd_mutex);
    OpenGLViewer::commands.push_back("mouse " + data);
    _self->viewer->postdisplay = true;
    pthread_mutex_unlock(&_self->viewer->cmd_mutex);
  }
  else
  {
    // No suitable handler found, mark as not processed. Mongoose will
    // try to serve the request.
    return 0;
  }

  // Returning non-zero tells mongoose that our function has replied to
  // the client, and mongoose should not send client any more data.
  return 1;
}

#endif  //DISABLE_SERVER
