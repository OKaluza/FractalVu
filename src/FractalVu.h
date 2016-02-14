//Fractal viewer
#ifndef FractalVu__
#define FractalVu__

#include "GraphicsUtil.h"
#include "ColourMap.h"
#include "Shaders.h"
#include "ViewerApp.h"
#include "FractalServer.h"
#include "FractalUtil.h"
#include "LavaVu.h"

class FractalVu : public LavaVu
{
  FractalShader* prog;
  GLuint vertexPositionBuffer;
  GLuint gradientTexture;
protected:
  //uniforms
  float pixelsize;
  std::vector<FilePath> files;
  ColourMap* colourMap;

public:
  bool newframe;
  FractalServer* server;
  std::string fragmentShader;
  std::string palette;
  //For tiled rendering...
  int tile[2];
  int tiles[2];
  float dims[2];

  json::Object properties;

  FractalVu(std::vector<std::string> args, OpenGLViewer* viewer, int width=0, int height=0);
  ~FractalVu();

  std::string run(bool persist);

  void parseProperties(std::string& properties);
  void parseProperty(std::string& data);
  void printProperties();

  void loadProgram();
  void loadPalette();

  //*** Our application interface:

  // Virtual functions for window management
  virtual void open(int width, int height);
  virtual void resize(int new_width, int new_height);
  virtual void display();
  virtual void close();

  void write_tiled(bool alpha, int count=0);

  // Virtual functions for interactivity
  virtual bool mouseMove(int x, int y);
  virtual bool mousePress(MouseButton btn, bool down, int x, int y);
  virtual bool mouseScroll(int scroll);
  virtual bool keyPress(unsigned char key, int x, int y);

  virtual bool parseCommands(std::string cmd);

  //***

  bool update(std::string data);

  void read(FilePath& fn);
  std::string stripSection(std::string name, std::string& params);
  bool parse(std::string& params);
  std::string stringify();
  void drawScene();
  void convert(float coord[2], float x, float y);

  void zoomSteps(bool images, int steps);
};

#endif
