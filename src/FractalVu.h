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

  Properties properties;

  FractalVu(std::string binpath, bool omegalib=false);
  ~FractalVu();

  void run(std::vector<std::string> args);

  void parseProperty(std::string& data);
  void printProperties();

  bool loadFile(const std::string& file);

  void loadProgram();
  void loadPalette();

  //*** Our application interface:

  // Virtual functions for window management
  virtual void open(int width, int height);
  virtual void resize(int new_width, int new_height);
  virtual void display(bool redraw=true);
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

  std::string stripSection(std::string name, std::string& params);
  bool parse(std::string& params);
  std::string stringify();
  void drawScene();
  void convert(float coord[2], float x, float y);

  void zoomSteps(bool images, int steps);
};

#endif
