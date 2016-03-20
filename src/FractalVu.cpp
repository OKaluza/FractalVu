#include "Include.h"
#include "FractalVu.h"
#include "Shaders.h"
#include "VideoEncoder.h"
#include "base64.h"

//Include the decompression routines
#ifdef USE_ZLIB
#include <zlib.h>
#else
#include "miniz/tinfl.c"
#endif

const char *fractalVertexShader = STRINGIFY(
                                    attribute vec3 aVertexPosition;
                                    uniform mat4 uMVMatrix;
                                    varying vec2 coord;
                                    void main(void)
{
  gl_Position = vec4(aVertexPosition, 1.0);
  //Apply translation, rotation & scaling matrix to vertices to get fractal space coords
  vec4 coords = uMVMatrix * vec4(aVertexPosition.xy, 0.0, 1.0);
  coord = coords.xy;
}
                                  );

const char *fractalFragmentShader = STRINGIFY(
                                      uniform vec2 offset;
                                      uniform int iterations;
                                      uniform bool julia;
                                      uniform float pixelsize;
                                      uniform vec2 dims;
                                      uniform vec2 origin;
                                      uniform vec2 selected_;
                                      uniform sampler2D palette;
                                      uniform vec4 background;
                                      uniform float params;
                                      varying vec2 coord;
                                      void main()
{
  //Globals
  vec2 z;
  vec2 c;
  vec2 point;            //Current point coord
  vec2 z_1;              //Value of z(n-1)
  vec2 z_2;              //Value of z(n-2)
  int count = 0;            //Step counter
  vec4 colour = background;
  point = coord + vec2(offset.x, offset.y);
  if (julia)
  {
    z = point;
    c = selected_;
  }
  else
  {
    z = vec2(0,0);
    c = point;
  }

  //Iterate the fractal formula
  for (int i=0; i < iterations; i++)
  {
    if (i == iterations) break;
    //Run next calc step
    count++;  //Current step count
    z = vec2(z.x*z.x - z.y*z.y, z.x*z.y + z.y*z.x) + c;
    if (dot(z,z) > 4.0)
    {
      colour = texture2D(palette, vec2(float(count) / float(iterations), 0.0));
      break;
    }
  }
  gl_FragColor = colour;
}
                                    );

int jsonToFloatArray(json& value, float* out)
{

  if (out == NULL) out = new float[value.size()];
  for (size_t i = 0; i < value.size(); i++)
    out[i] = (float)value[i];
  return value.size();
}

json jsonFromFloatArray(float* in, int count)
{
  json array;
  for (size_t i = 0; i < count; i++)
    array.push_back(in[i]);
  return array;
}

//Viewer class implementation...
FractalVu::FractalVu(std::string path) : LavaVu(), binpath(path)
{
  FilePath* fractalfile = NULL;
  encoder = NULL;

  tiles[0] = tiles[1] = 4;
  newframe = true;

  //Clear shader path
  Shader::path = "";
  vertexPositionBuffer = 0;
  gradientTexture = 0;
  prog = NULL;
  server = NULL;
  colourMap = NULL;

  //Set info/error stream
  if (verbose && !output)
    infostream = stderr;

  //Tiled render disabled by default
  tile[0] = tile[1] = -1;
}

FractalVu::~FractalVu()
{
  if (colourMap) delete colourMap;
}

std::string FractalVu::run()
{
  Properties::defaults["iterations"]  = 100;
  Properties::defaults["zoom"]        = 0.5;
  Properties::defaults["rotate"]      = 0.5;
  Properties::defaults["julia"]       = false;
  Properties::defaults["origin"]      = {0., 0.};
  Properties::defaults["selected"]    = {0., 0.};
  Properties::defaults["shift"]       = {0., 0.};
  Properties::defaults["background"]  = {0.,0.,0.,1.};
  Properties::defaults["antialias"]   = 2;
  std::cout << std::setw(2) << Properties::defaults << std::endl;

////////////////////////////////////////////////////////////
  int width = 0, height = 0;

  if (properties.has("width"))
    width = properties["width"];
  if (properties.has("height"))
    height = properties["height"];

  viewer->width = 0;
  viewer->height = 0;

  if (width) viewer->width = width;
  if (height) viewer->height = height;
  dims[0] = viewer->width;
  dims[1] = viewer->height;

  //Add input/output attachments to the viewer
  int port = 8080, quality = 0, threads = 1;
  std::string htmlpath = GetBinaryPath(binpath.c_str(), "FractalVu") + "html";
  server = FractalServer::Instance(viewer, htmlpath, port, quality, threads);
  viewer->addOutput(server);
#ifdef USE_MIDI
  MidiInput stdi;
  viewer->addInput(&stdi);
#endif

////////////////////////////////////////////////////////////

  //Serve images as remote renderer only
  server->imageserver = !writemovie && !viewer->visible;

  //Start event loop
  viewer->open(viewer->width, viewer->height);

  if (server->imageserver)
  {
    //Non-interactive mode for rendering images as server
    //No local user input processing
    while (!viewer->quitProgram)
    {
      //Wait until woken by server
      pthread_mutex_lock(&server->p_mutex);
      debug_print("@@ %u DISPLAY Mutex locked\n", pthread_self());
      pthread_t tid;
      tid = pthread_self();

      while (!viewer->postdisplay && !viewer->quitProgram)
      {
        debug_print("@@ CLIENT THREAD ID %u WAITING\n", tid);
        pthread_cond_wait(&FractalServer::p_condition_var, &server->p_mutex);
      }
      debug_print("@@ CLIENT THREAD ID %u RESUMED, quit? %d\n", tid, viewer->quitProgram);

      pthread_mutex_unlock(&server->p_mutex);
      debug_print("@@ %u DISPLAY Mutex unlocked\n---------------------------\n", pthread_self());

      newframe = true;
      viewer->display();
    }
  }
  else
    //Use standard event processing loop
    viewer->execute();
 
  FractalServer::Delete();
  return "";
}

//Property containers now using json
//Parse lines with delimiter, ie: key=value
void FractalVu::parseProperties(std::string& properties)
{
  //Process all lines
  std::stringstream ss(properties);
  std::string line;
  while(std::getline(ss, line))
    parseProperty(line);
};

void FractalVu::parseProperty(std::string& data)
{
  //Set properties global object
  properties.parse(data);
}

void FractalVu::printProperties()
{
  //Show properties of selected object or view/globals
  std::cerr << "DATA: " << properties.data << std::endl;
}

void FractalVu::read(FilePath& fn)
{
  //Parse fractal file
  std::ifstream fs(fn.full.c_str(), std::ios::in);
  if (fs.is_open())
  {
    std::stringstream buffer;
    buffer << fs.rdbuf();
    //std::cout << buffer.str() << std::endl;
    update(buffer.str());
    fs.close();
  }
}

std::string FractalVu::stripSection(std::string name, std::string& params)
{
  //Find section "name" in params, strip out and return contents
  //Removes all content after this tag in params so relies on being called
  //on sections at bottom first
  std::size_t pos = params.find(name);
  if (pos != std::string::npos)
  {
    //Strip section
    std::string content = params.substr(pos + name.length() + 1);
    params = params.substr(0, pos);
    return content;
  }
  return std::string("");
}

bool FractalVu::parse(std::string& params)
{
  //Strip sections from bottom up
  bool palette_changed = false;
  //Strip palette section
  std::string newpalette = stripSection("[palette]", params);
  if (newpalette.length() != palette.length() || newpalette != palette)
  {
    palette = newpalette;
    palette_changed = true;
  }

  //Parse remaining params
  size_t paren = params.find('{');
  if (paren != std::string::npos)
  {
    //Already JSON
    properties.parse(params.substr(paren));
  }
  else
  {
    //param=value with brackets for arrays
    std::replace(params.begin(), params.end(), '(', '[');
    std::replace(params.begin(), params.end(), ')', ']');
    properties.parseSet(params);
  }

  //Resizing is allowed only when using server in remote render mode
  if (FractalServer::images)
  {
    dims[0] = properties.getInt("width", viewer->width);
    dims[1] = properties.getInt("height", viewer->height);
  }

  return palette_changed;
}

std::string FractalVu::stringify()
{
  std::stringstream os;
  os << "[fractal]\n" << properties.data;
  os << "\n[palette]\n" << palette;
  if (fragmentShader.length())
    os << "[shader]\n" << fragmentShader;
  return os.str();
}

void FractalVu::open(int width, int height)
{
  //Read files
  if (files.size() > 0 && files[0].ext != "")
  {
    //Read params
    read(files[0]);
    viewer->title = files[0].base;
    std::string shaderpath;
    //Read shader
    if (files.size() > 1)
      read(files[1]);
    //Assume shader exists with same base name if none already loaded
    if (fragmentShader.length() == 0)
    {
      FilePath shaderpath = FilePath(files[0].path + files[0].base + ".shader");
      read(shaderpath);
    }
  }

  //Viewer window created
  printf("VIEWER OPENED %p %d,%d - %d,%d\n", this, width, height, viewer->width, viewer->height);
  resize(width, height);

  if (!gradientTexture && width > 0)
    glGenTextures(1, &gradientTexture);

  //A default setup for when no file passed
  if (fragmentShader.length() == 0)
  {
    if (viewer->width) loadProgram();
    debug_print("DEFAULT SHADER LOADED: %p %d %p\n", this, viewer->isopen, prog);
    //Load default palette
    palette = std::string("Background=rgba(0,0,0,0)\n0.0=rgba(0,0,0,1)\n0.5=rgba(255,255,255,1)\n1.0=rgba(0,0,0,1)\n");
    loadPalette();
  }
}

void FractalVu::loadProgram()
{
  //Program reloaded, get buffers etc
  if (!prog)
  {
    if (fragmentShader.length() > 0)
      prog = new FractalShader(fragmentShader);
    else
      prog = new FractalShader();
    if (!prog->program) abort_program("Shader build failed: %s\n", fragmentShader.c_str());
  }
  else
  {
    //Rebuild
    if (!prog->compile(fragmentShader.c_str(), GL_FRAGMENT_SHADER))
    {
      debug_print("Compile failed!\n%s", fragmentShader.c_str());
      return;
    }
    prog->build();
  }

  const char* uniforms[11] = {"uMVMatrix", "palette", "offset", "iterations", "julia", "origin", "selected_", "dims", "pixelsize", "background", "params"};
  prog->loadUniforms(uniforms, 11);
  prog->use(); //Activate shader

  if (!vertexPositionBuffer)
  {
    //All output drawn onto a single 2x2 quad
    glGenBuffers(1, &vertexPositionBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexPositionBuffer);
    float vertexPositions[8] = {1.0,1.0,  -1.0,1.0,  1.0,-1.0,  -1.0,-1.0};
    glBufferData(GL_ARRAY_BUFFER, 8*sizeof(float), vertexPositions, GL_STATIC_DRAW);
  }
}

void FractalVu::loadPalette()
{
  //Parse string into key/value pairs
  std::stringstream is(palette);
  //printf("PALETTE LOAD %p\n", this);
#define PALSIZE 4096
  if (colourMap) delete colourMap;
  colourMap = new ColourMap();
  std::string line;
  bool bg = true;
  while(std::getline(is, line))
  {
    if (bg)
    {
      //Background
      std::size_t pos = line.find("=") + 1;
      Colour c = Colour_FromString(line.substr(pos));
      properties.data["background"] = Colour_ToJson(c);
      bg = false;
      continue;
    }
    std::istringstream iss(line);
    float pos;
    char delim;
    std::string value;
    if (iss >> pos && pos >= 0.0 && pos <= 1.0)
    {
      iss >> delim;
      iss >> value;
      Colour colour = Colour_FromString(value);
      //Add to colourmap
      colourMap->add(colour.value, PALSIZE * pos);
    }
  }

  colourMap->calibrate(0, PALSIZE);
  unsigned char paletteData[4*PALSIZE];
  Colour col;
  for (int i=0; i<PALSIZE; i++)
  {
    //col = colourMap->get(i);
    col = colourMap->getfast(i);
    //printf("RGBA %d %d %d %d\n", col.r, col.g, col.b, col.a);
    paletteData[i*4] = col.r;
    paletteData[i*4+1] = col.g;
    paletteData[i*4+2] = col.b;
    paletteData[i*4+3] = col.a;
  }

  if (gradientTexture)
  {
    glBindTexture(GL_TEXTURE_2D, gradientTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PALSIZE, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, paletteData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
}

void FractalVu::close()
{
}

void FractalVu::resize(int new_width, int new_height)
{
  dims[0] = new_width;
  dims[1] = new_height;
  glScissor(0, 0, new_width, new_height);
}

// Render
void FractalVu::display(void)
{
  if (viewer->width == 0) return;

  //Server update?
  //printf("[%d tile %d] -- %d commands viewer %p app %p\n", context.tile->isInGrid, context.tile->id, FractalServer::data.size(), viewer, glapp);
  if (FractalServer::data.size() > 0)
  {
    std::string cmd = FractalServer::data.front();
    FractalServer::data.pop_front();
    update(cmd);
    newframe = true;  //Frame data changed
    //More queued?
    if (FractalServer::data.size() > 0)
      viewer->postdisplay = true;  //Display queued
  }

  //Only update frame if changed...
  //!!!!!!!!!this kills server rendering and doesn't work with omegalib
  //if (newframe)
  {
    prog->use(); //Activate shader
    // Clear viewport
    glDrawBuffer(viewer->renderBuffer);
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GL_Error_Check;

    switch (viewer->blend_mode)
    {
    case BLEND_NORMAL:
      // Blending setup for interactive display...
      // Normal alpha blending for rgb colour, accumulate opacity in alpha channel with additive blending
      glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA);
      //Render!
      drawScene();
      break;
    case BLEND_PNG:
      // Blending setup for write to transparent PNG...
      // This works well but with some blend errors that show if only rendering a few particle layers
      // Rendering colour only first pass at fully transparent, then second pass renders as usual
      glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ZERO, GL_ZERO);
      drawScene();
      // Clear the depth buffer so second pass is blended or nothing will be drawn
      glClear(GL_DEPTH_BUFFER_BIT);
      glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      drawScene();
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      break;
    }

    glUseProgram(0);
    newframe = false;
  }
  //else
  //{
  //TODO: redraw copied framebuffer for omegalib render...
  //}

#ifdef HAVE_LIBAVCODEC
  if (encoder)
  {
    viewer->pixels(encoder->buffer, false, true);
    //bitrate settings?
    encoder->frame();
  }
  else if (writemovie)
  {
    if (properties.has("name"))
    {
      //Start video output 
      std::string name = properties["name"];
      encodeVideo(name + ".mp4", writemovie);
    }
  }
#endif
}

void FractalVu::drawScene()
{
  float origin[2], selected[2], shift[2];
  jsonToFloatArray(properties["origin"], origin);
  jsonToFloatArray(properties["selected"], selected);
  jsonToFloatArray(properties["shift"], shift);
  selected[0] += shift[0];
  selected[1] += shift[1]; //Apply shift
  float zoom = properties["zoom"];
  float rotate = properties["rotate"];
  int iterations = properties["iterations"];
  bool julia = properties["julia"];
  Colour background = properties.getColour("background", 0, 0, 0, 255);
  int antialias = properties["antialias"];

  //Save origin/zoom (modified by tiling calcs)
  float origin0[2] = {origin[0], origin[1]};
  float zoom0 = zoom;

  //Render as tile?
  if (tile[0] >= 0 && tile[1] >= 0)
  {
    //Save height then truncate to aspect ratio of full display
    //float height = viewer->height;
    //float aspect = dims[0] / (float)dims[1];
    //viewer->height = viewer->width / aspect;

    float inc[2] = {viewer->width / (float)tiles[0], viewer->height / (float)tiles[1]};
    float re = inc[0]/2.0 + tile[0] * inc[0];
    float im = inc[1]/2.0 + tile[1] * inc[1];
    //Convert centre pixel coords to fractal coords
    //std::cout << "Viewer offset: " << inc[0] << " -> " << re << std::endl;
    convert(origin, re, im);
    //std::cout << "Viewer tiles: " << tile[0] << "," << tile[1] << " of " << tiles[0] << "," << tiles[1] << std::endl;

    //Fractal is scaled to height so use height ratio to calc zoom
    zoom *= dims[1] / (float)viewer->height;
  }

  //glDisable(GL_MULTISAMPLE);
  glEnable(GL_MULTISAMPLE);

  //Restore state
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_LIGHTING);
  GL_Error_Check;

//Fractal.draw
  assert(prog);

  //Uniform variables
  glUniform1i(prog->uniforms["iterations"], iterations);
//std::cout << "Iterations: " << iterations << std::endl;
  glUniform1i(prog->uniforms["julia"], (bool)properties["julia"]);
//std::cout << "Julia: " << julia << std::endl;
  Colour_SetUniform(prog->uniforms["background"], background);
  glUniform2fv(prog->uniforms["origin"], 1, origin);
  glUniform2fv(prog->uniforms["selected_"], 1, selected);
//std::cout << "Selected: " << selected[0] << "," << selected[1] << std::endl;
  glUniform2fv(prog->uniforms["dims"], 1, dims);
//std::cout << "Dims: " << dims[0] << "," << dims[1] << std::endl;

  //Update pixel size in fractal coords
  pixelsize = 2.0 / ((float)properties["zoom"] * viewer->width);
  glUniform1f(prog->uniforms["pixelsize"], pixelsize);
  GL_Error_Check;

  //Gradient texture
  glBindTexture(GL_TEXTURE_2D, gradientTexture);
  glUniform1i(prog->uniforms["palette"], 0);

  //Parameter variables (uniforms/buffer)
  if (properties.has("variables"))
  {
    float params[64];
    int pcount = jsonToFloatArray(properties["variables"], params);
    glUniform1fv(prog->uniforms["params"], pcount, &params[0]);
  }

  //Apply translation to origin, any rotation and scaling (inverse of zoom factor)
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(origin[0], origin[1], 0);
  glRotatef(rotate, 0.0, 0.0, -1.0);
  //Apply zoom and flip Y to match old coord system
  glScalef(1.0/zoom, -1.0/zoom, 1.0);
  //Scaling to preserve fractal aspect ratio
  //std::cout << "Viewer size: " << viewer->width << "," << viewer->height << std::endl;
  //std::cout << "Total size: " << dims[0] << "," << dims[1] << std::endl;
  if (tile[0] >= 0 || viewer->width > viewer->height)
    glScalef(viewer->width/(float)viewer->height, 1.0, 1.0);  //Scale width
  else if (viewer->height > viewer->width)
    glScalef(1.0, viewer->height/(float)viewer->width, 1.0);  //Scale height

  //Get the matrix to send as uniform data
  float mvMatrix[16];
  glGetFloatv(GL_MODELVIEW_MATRIX, mvMatrix);
  GL_Error_Check;


  glClearColor(background.r/255.0, background.g/255.0, background.b/255.0, background.a/255.0);
  glEnable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);  //No depth testing to allow multi-pass blend!
  GL_Error_Check;

  glViewport(0, 0, viewer->width, viewer->height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  GL_Error_Check;

  glBindBuffer(GL_ARRAY_BUFFER, vertexPositionBuffer);
  glVertexAttribPointer(prog->vertexPositionAttribute, 2, GL_FLOAT, false, 0, 0);
  GL_Error_Check;

  //Rotation & translation matrix
  glUniformMatrix4fv(prog->uniforms["uMVMatrix"], 1, GL_FALSE, mvMatrix);
  GL_Error_Check;

  //Draw and blend multiple passes for anti-aliasing
  glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
  float blendinc = 0;
  float pixel_x = 2.0 / (zoom * dims[0]);
  float pixel_y = 2.0 / (zoom * dims[1]);
  for (int j=0; j<antialias; j++)
  {
    for (int k=0; k<antialias; k++)
    {
      float offset_x = pixel_x * (j/(float)antialias-0.5);
      float offset_y = pixel_y * (k/(float)antialias-0.5);
      float blendval = 1.0 - blendinc;
      //printf("Antialias pass ... %d - %d blendinc: %f blendval: %f bv2: %f offset: %f,%f\n",
      //       j, k, blendinc, blendval, pow(blendval, 1.5), offset_x, offset_y);
      //blendval *= blendval;
      blendval = pow(blendval, 1.5);
      glBlendColor(blendval, blendval, blendval, blendval);
      blendinc += 1.0/(antialias*antialias);

      glUniform2f(prog->uniforms["offset"], offset_x, offset_y);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
  }

  GL_Error_Check;

  //Restore pre-tiled
  zoom = zoom0;
  origin[0] = origin0[0];
  origin[1] = origin0[1];
}

//Converts a set of pixel coords to
// a new fractal pos based on current fractal origin, zoom & rotate...
void FractalVu::convert(float coord[2], float x, float y)
{
  float zoom = properties["zoom"];
  float rotate = properties["rotate"];

  float half_w = viewer->width * 0.5;
  float half_h = viewer->height * 0.5;

  //Scale based on smallest dimension and aspect ratio
  //(Use dims which refers to the full image size when rendering tiles)
  float box = dims[0] < dims[1] ? dims[0] : dims[1];
  if (tile[0] >= 0) box = dims[1];
  float scalex = dims[0] / box;
  float scaley = dims[1] / box;

  float re = scalex * (x - half_w) / (half_w * zoom);
  float im = scaley * (y - half_h) / (half_h * zoom);

  //Apply rotation
  float arad = -rotate * DEG2RAD;
  float Cos = cos(arad);
  float Sin = sin(arad);
  coord[0] += re*Cos - im*Sin;
  coord[1] += re*Sin + im*Cos;
}

void FractalVu::write_tiled(bool alpha, int count)
{
#ifdef HAVE_LIBPNG
  std::string fn = (files.size() > 0 ? files[0].base : "default");
  float zoom = properties["zoom"];
  //Tiled image
  int width = viewer->width * tiles[0];
  int height = viewer->height * tiles[1];
  int pixel = alpha ? 4 : 3;

  //Write tiled data to single image file
  char path[256];
  sprintf(path, "%s%s-tiled-%d.png", viewer->output_path.c_str(), fn.c_str(), count);
  std::ofstream file(path, std::ios::binary);

  //Set final image dims
  dims[0] = width;
  dims[1] = height;

  //Adjust output size
  //viewer->outwidth = viewer->width;
  //viewer->outheight = viewer->height * tiles / (float)tiles;

  //PNG export requires all data at once
  int tscanline = pixel * width;
  GLubyte *image, *ptr;
  image = new GLubyte[width * height * pixel];
  ptr = image + tscanline * height;  //Flip, start at bottom for png

  GLubyte *tile = new GLubyte[viewer->width * viewer->height * pixel];

  //Render & write tiles
  for (int row=0; row < tiles[1]; row++)
  {
    ptr -= tscanline * viewer->height;
    for (int col=0; col < tiles[0]; col++)
    {
      tile[0] = col;
      tile[1] = row;

      viewer->display();

      // Read the pixels
      viewer->pixels(tile, alpha);

      //Copy scanlines into total image
      int scanline = pixel * viewer->width;
      GLubyte* dst = ptr + scanline * col + viewer->height * tscanline;

      //Flipped for png output
      GLubyte* src = tile + viewer->height * scanline;
      for (int y=0; y<viewer->height; y++)
      {
        dst -= tscanline;
        src -= scanline;
        memcpy(dst, src, scanline);
      }
    }
  }

  write_png(file, alpha, dims[0], dims[1], image);
  file.close();

  //Restore image dims
  dims[0] = viewer->width;
  dims[1] = viewer->height;
  tile[0] = tile[1] = -1;

  printf("Width: %d Height: %d\n", width, height);
  delete[] image;
  delete[] tile;
#endif
}

/////////////////////////////////////////////////////////////////////////////////
// Event handling
/////////////////////////////////////////////////////////////////////////////////
bool FractalVu::keyPress(unsigned char key, int x, int y)
{
  //Process single char commands
  switch(key)
  {
  case KEY_UP:
    break;
  case KEY_DOWN:
    break;
  case KEY_RIGHT:
    properties.data["antialias"] = (int)properties["antialias"] + 1;
    break;
  case KEY_LEFT:
    properties.data["antialias"] = (int)properties["antialias"] - 1;
    break;
  case KEY_PAGEUP:
    break;
  case KEY_PAGEDOWN:
    break;
  case KEY_HOME:
    break;
  case KEY_END:
    break;
  case 'm':
    //Start video output 
    {
      std::string name = properties["name"];
      encodeVideo(name + ".mp4");
    }
    break;
  case 't':
    write_tiled(false);
    break;
  case '`':
    viewer->fullScreen();
    break;
  case KEY_ESC:
  case 'q':
    viewer->quitProgram = true;
    return false;
    break;
  case 's':
    viewer->snapshot();
    break;
  case 'z':
    //images, steps
    zoomSteps(false, 1800);
    break;
  }
}

bool FractalVu::mousePress(MouseButton btn, bool down, int x, int y)
{
  //Only process on mouse release
  static float rotate[3], translate[3], stereo[3];
  bool redraw = false;
  int scroll = 0;
  if (down)
  {
    //if (viewPorts) viewSelect(viewFromPixel(x, y));  //Update active viewport
    viewer->button = btn;
    viewer->last_x = x;
    viewer->last_y = y;
    //aview->get(rotate, translate, stereo);   //Get current rotation & translation
  }
  else
  {
    //aview->inertia(INERTIA_OFF); //Reset inertia lag

    switch (viewer->button)
    {
    case WheelUp:
      scroll = 1;
      redraw = true;
      break;
    case WheelDown:
      scroll = -1;
      redraw = true;
      break;
    case LeftButton:
    {
      //aview->rotated = true;  //Flag rotation finished
      //Convert coord and set origin
      float origin[2];
      jsonToFloatArray(properties["origin"], origin);
      convert(origin, x, y);
      printf("%d,%d origin %f,%f \n", x, y, origin[0], origin[1]);
      properties.data["origin"] = jsonFromFloatArray(origin, 2);
      redraw = true;
      break;
    }
    case MiddleButton:
      break;
    case RightButton:
      break;
    }

    //Process wheel scrolling
    if (scroll) mouseScroll(scroll);

    viewer->button = NoButton;
  }
  return redraw;
}

bool FractalVu::mouseMove(int x, int y)
{
  float adjust;
  int dx = x - viewer->last_x;
  int dy = y - viewer->last_y;
  viewer->last_x = x;
  viewer->last_y = y;

  //For mice with no right button, ctrl+left
  if (viewer->keyState.ctrl && viewer->button == LeftButton)
    viewer->button = RightButton;

  switch (viewer->button)
  {
  case LeftButton:
    // left = rotate
    //aview->rotate(dy / 5.0f, 0.0f, 0.0f);
    //aview->rotate(0.0f, dx / 5.0f, 0.0f);
    break;
  case RightButton:
    // right = translate
    //adjust = aview->model_size / 200.0;   //1/200th of size
    //aview->translate(dx * adjust, -dy * adjust, 0);
    break;
  case MiddleButton:
    // middle = rotate z (roll)
    //aview->rotate(0.0f, 0.0f, dx / 5.0f);
    break;
  default:
    return false;
  }

  //aview->inertia(INERTIA_ON);
  return true;
}

bool FractalVu::mouseScroll(int scroll)
{
  //CTRL+ALT+SHIFT
  if (viewer->keyState.alt && viewer->keyState.shift && viewer->keyState.ctrl)
    ;
  //ALT+SHIFT
  if (viewer->keyState.alt && viewer->keyState.shift)
    ;
  //SHIFT
  else if (viewer->keyState.shift)
    properties.data["iterations"] = (int)properties["iterations"] + scroll;
  //ALT
  else if (viewer->keyState.alt)
    ;
  //CTRL
  else if (viewer->keyState.ctrl)
  {
    if (scroll < 0)
      properties.data["zoom"] = (float)properties["zoom"] * (1/(-scroll * 1.01));
    else
      properties.data["zoom"] = (float)properties["zoom"] * (scroll * 1.01);
  }
  //Default = zoom
  else
  {
    if (scroll < 0)
      properties.data["zoom"] = (float)properties["zoom"] * (1/(-scroll * 1.1));
    else
      properties.data["zoom"] = (float)properties["zoom"] * (scroll * 1.1);
  }
  return true;
}

void FractalVu::zoomSteps(bool images, int steps)
{
  for (int i=0; i<=steps; i++)
  {
    std::cout << "... Writing step: " << i << " zoom: " << (float)properties["zoom"] << std::endl;
    viewer->display();
    if (images)
      viewer->snapshot(-1, viewer->alphapng);

    write_tiled(false, i);

    properties.data["zoom"] = (float)properties["zoom"] * 1.003;
    //if (i%10 == 0) iterations+= 7;
  }
}

bool FractalVu::parseCommands(std::string data)
{

  //float selected[2] = {properties["selected"].ToArray()[0].ToFloat(0), properties["selected"].ToArray()[1].ToFloat(0)};
  float selected[2];
  jsonToFloatArray(properties["selected"], selected);
  //json::Array selected = properties["selected"].ToArray();

  //Data contains a fractal definition? Load it
  if (data.substr(0, 9) == "[fractal]")
  {
    update(data);
    return true;
  }

  //Otherwise parse as commands
  if (data == "redisplay")
  {
    if (viewer->visible)
    {
      viewer->display();
      //server->render();
    }
    else
      return true;
  }
  else
  {
    std::stringstream ss(data);
    PropertyParser parsed = PropertyParser(ss);
    float fval = 0;
    if (parsed.has(fval, "scroll"))
    {
      if (fval < 0)
        properties.data["zoom"] = (float)properties["zoom"] * (1/(-fval * 1.01));
      else
        properties.data["zoom"] = (float)properties["zoom"] * (fval * 1.01);
    }
    else if (parsed.has(fval, "translatex"))
    {
      selected[0] += fval;
      properties.data["selected"] = jsonFromFloatArray(selected, 2);
    }
    else if (parsed.has(fval, "translatey"))
    {
      //selected[1] = origsel[1] + fval;
      selected[1] += fval;
      properties.data["selected"] = jsonFromFloatArray(selected, 2);
      //selected[1] += fval;
    }
    else if (parsed.has(fval, "translatez"))
    {
      properties.data["zoom"] = (float)properties["zoom"] + fval;

      /*if (scroll < 0)
        zoom *= (1/(-scroll * 1.01));
      else
        zoom *= (scroll * 1.01);*/
    }
    else if (parsed.has(fval, "rotatex"))
    {
      //properties["variables"][0] = properties["variables"][0].ToFloat(0.0) + fval;
    }
    else if (parsed.has(fval, "rotatey"))
    {
      //properties["variables"][1] = properties["variables"][1].ToFloat(0.0) + fval;
    }
    else if (parsed.has(fval, "rotatez"))
    {
      properties.data["rotate"] = (float)properties["rotate"] + fval;
      //std::cout << "RECEIVED VAL " << fval << std::endl;
      //std::cout << "selected " << selected[0] << "," << selected[1] << std::endl;
    }
    else
    {
      std::size_t found = data.find("=");
      if (found == std::string::npos)
      {
        //std::cerr << "# Unrecognised command: \"" << data << "\"" << std::endl;
        //return false;  //Invalid
        return LavaVu::parseCommands(data);
      }
      else
      {
        parseProperty(data);
      }
    }

    return true;
  }

  return false;
}

bool FractalVu::update(std::string data)
{
//std::cout << "UPDATING...\n";
  //std::string params, shader;
  //Strip shader, params sections
  std::string shader = stripSection("[shader]", data);
  std::string params = stripSection("[fractal]", data);
  /*if (shader.length() == 0 && params.length() == 0)
  {
    //Assume shader only
    shader = data;
  }*/

  //Rebuild new shader
  bool shaderChanged = (shader.length() != fragmentShader.length() || shader != fragmentShader);
  debug_print("shaderChanged? %d: %d %d params: %d\n", shaderChanged, shader.length(), fragmentShader.length(), params.length());
  if (shaderChanged)
  {
    //Save source
    fragmentShader = shader;
    //Reload uniforms/buffers
    //debug_print("NEW SHADER LOADING: %p %d %d,%d\n", this, viewer->isopen, viewer->width, viewer->height);
    if (viewer->width) loadProgram();
    //else debug_print("SKIPPED SHADER! NO PROGRAM %p\n", this);
    //debug_print("NEW SHADER LOADED: %p %d %p\n", this, viewer->isopen, prog);
  }

  //Reload the new params
  if (params.length())
  {
    if (parse(params))
      //Reload palette
      loadPalette();

    //Resized by remote? (only happens in remote render mode)
    if (dims[0] != viewer->width || dims[1] != viewer->height)
    {
      debug_print("### Resized: %p %d,%d\n", this, dims[0], dims[1]);
      if (viewer->fbo_enabled)
      {
        viewer->setsize(dims[0], dims[1]);
        //Manually call resize as no window manager to call it
        viewer->resize(dims[0], dims[1]);
      }
      else
      {
        viewer->setsize(dims[0], dims[1]);
        FractalServer::resized = true; //Flag resize pending - used to prevent frame send before resize completed
      }
    }
  }
  return shaderChanged;
}
