/********************************************************************************************************************** 
 * Fractal Viewer in OmegaLib
 *********************************************************************************************************************/
#ifdef USE_OMEGALIB

#include <omega.h>
#include <omegaGl.h>
#include <omegaToolkit.h>
#include "../src/ViewerApp.h"
#include "../src/FractalViewer.h"
#include "../src/FractalServer.h"

std::vector<std::string> args;

using namespace omega;
using namespace omegaToolkit;
using namespace omegaToolkit::ui;

class FractalApplication;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FractalRenderPass: public RenderPass, ViewerApp
{
public:
  FractalRenderPass(Renderer* client, FractalApplication* app, OpenGLViewer* viewer)
                  : RenderPass(client, "FractalRenderPass"), app(app), ViewerApp(viewer), server(NULL) {}
  virtual void initialize();
  virtual void render(Renderer* client, const DrawContext& context);

   // Virtual functions for interactivity (from ViewerApp/ApplicationInterface)
   virtual bool mouseMove(int x, int y) {}
   virtual bool mousePress(MouseButton btn, bool down, int x, int y) {}
   virtual bool mouseScroll(int scroll) {}
   virtual bool keyPress(unsigned char key, int x, int y) {}

private:
  FractalApplication* app;
  FractalViewer* glapp;
  FractalServer* server;
  bool redisplay;

};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FractalApplication: public EngineModule
{
public:
  FractalApplication(): EngineModule("FractalApplication")
  {
    DisplaySystem* ds = getEngine()->getDisplaySystem();
    Vector2i resolution = ds->getCanvasSize();

    //Master viewer (does no actual rendering but stores parameters)
    master = new FractalViewer(args, new OpenGLViewer(false, false), 0, 0);
   printf("CREATED MASTER VIEWER %p\n", master);
    //Open to read data but don't use OpenGL
    master->open(resolution[0], resolution[1]);

    enableSharedData(); 
    srand(time(NULL));
  }

  virtual void initialize()
  {
    DisplaySystem* ds = SystemManager::instance()->getDisplaySystem();
    // Create and initialize the UI management module.
    myUiModule = UiModule::createAndInitialize();
    myUi = myUiModule->getUi();

    myMainLabel = Label::create(myUi);
    myMainLabel->setText("");
		//myMainLabel->setColor(Color::Gray);
    int sz = 200;
    myMainLabel->setFont(ostr("fonts/arial.ttf %1%", %sz));
    myMainLabel->setHorizontalAlign(Label::AlignLeft);
    myMainLabel->setPosition(Vector2f(100,100));
  }

  virtual void initializeRenderer(Renderer* r) 
  { 
    OpenGLViewer* viewer = new OpenGLViewer(false, false);
    r->addRenderPass(new FractalRenderPass(r, this, viewer));
  }

  virtual void handleEvent(const Event& evt);
  virtual void commitSharedData(SharedOStream& out);
  virtual void updateSharedData(SharedIStream& in);

  FractalViewer* master;
private:
  // The ui manager
  Ref<UiModule> myUiModule;
  // The root ui container
  Ref<Container> myUi;  
  //Widgets
  Ref<Label> myMainLabel;
  std::string labelText;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FractalRenderPass::initialize()
{
  RenderPass::initialize();

  //Init fractal app
  DisplaySystem* ds = app->getEngine()->getDisplaySystem();
  Vector2i resolution = ds->getCanvasSize();

  //Create the app
  glapp = new FractalViewer(args, viewer, resolution[0], resolution[1]);
   printf("CREATED VIEWER %p %p\n", glapp, viewer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FractalRenderPass::render(Renderer* client, const DrawContext& context)
{
  //if(context.task == DrawContext::SceneDrawTask)
  if(context.task == DrawContext::OverlayDrawTask)
  {
    if (!viewer->isopen)
    {
      DisplaySystem* ds = app->getEngine()->getDisplaySystem();
      Vector2i resolution = ds->getCanvasSize();
      //Setup tile render adjustments
      if (context.tile->isInGrid)
      {
        viewer->open(context.tile->pixelSize[0], context.tile->pixelSize[1]);
   printf("OPENING TILE %p %p (%d x %d)\n", glapp, viewer, context.tile->pixelSize[0], context.tile->pixelSize[1]);

        //app->master = glapp; //Copy to master ref
        float tiles[2] = {resolution[0] / context.tile->pixelSize[0], resolution[1] / context.tile->pixelSize[1]};
        glapp->tiles[0] = tiles[0];
        glapp->tiles[1] = tiles[1];
        glapp->tile[0] = context.tile->gridX;
        glapp->tile[1] = context.tile->gridY;
        glapp->dims[0] = resolution[0];
        glapp->dims[1] = resolution[1];
        std::cout << context.tile->id <<  " : Tiled config: " << glapp->tile[0] << "," << glapp->tile[1] << " of " << tiles[0] << "," << tiles[1] << std::endl;
      }
      else
      {
        std::cout << "Master window : " << context.tile->pixelSize[0] << " x " << context.tile->pixelSize[1] << std::endl;
        app->master = glapp;  //Use this window as master reference
        viewer->open(context.tile->pixelSize[0], context.tile->pixelSize[1]);
   printf("OPENING MASTER %p %p\n", glapp, viewer);

         //Start server!
         server = FractalServer::Instance(viewer, "./html", 8080, 95, 1);
      }
      printf("[%d tile %d] -- viewer %p glapp %p master %p\n", context.tile->isInGrid, context.tile->id, viewer, glapp, app->master);
      redisplay = true;
    }

    //Server update?
    //printf("[%d tile %d] -- %d commands viewer %p app %p\n", context.tile->isInGrid, context.tile->id, FractalServer::data.size(), viewer, glapp);
    if (FractalServer::data.size() > 0)
    {
      std::string cmd = FractalServer::data.front();
      FractalServer::data.pop_front();
      glapp->update(cmd);
      redisplay = true;
      //printf("%d\n", FractalServer::data.size());
    }

    //Params updated?
    std::string p1 = app->master->stringify();
    std::string p2 = glapp->stringify();
    if (p1.length() != p2.length() || p1 != p2)
    {
       glapp->update(p1);
       //std::cout << context.tile->isInGrid << "," << context.tile->id << " PARAMS: " << p1 << std::endl;
       //std::cout << context.tile->isInGrid << "," << context.tile->id << " PARAMS UPDATED" << p1.length() << " != " << p2.length() << std::endl;
       redisplay = true;
    }

    //Can't render on demand until I work out a way to 
    //prevent the glClear calls before rendering
    //if (FractalServer::data.size() == 0 || redisplay)
    {
      client->getRenderer()->beginDraw3D(context);
      //std::cout << context.tile->isInGrid << "," << context.tile->id << " my shader id: " << shaderId << std::endl;
      GL_Error_Check;

      //std::cout << "Viewer " <<  viewer << " App " << app << " GlApp " << glapp << " Julia? " << glapp->julia << std::endl;
      //viewer->mouseScroll(1);
      //glapp->viewer->mouseScroll(1);
      //std::cout << "DISPLAY: " << this << std::endl;
      viewer->display();
      redisplay = false;

      client->getRenderer()->endDraw();
      GL_Error_Check;
      glapp->newframe = true; //Force new frame
    }
  }

}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FractalApplication::handleEvent(const Event& evt)
{
  //if (!viewer) {printf("FAIL this: %p \n", this); return;}
  //printf("%d %d %d this: %p\n", evt.getType(), evt.getServiceType(), evt.getFlags(), this);
  int flags = evt.getFlags();
  if (evt.getServiceType() == Service::Pointer)
  {
    int x = evt.getPosition().x();
    int y = evt.getPosition().y();
    MouseButton button = NoButton;
    if (flags & 1)
      button = LeftButton;
    else if (flags & 2)
      button = RightButton;
    else if (flags & 4)
      button = MiddleButton;

    switch (evt.getType())
    {
      case Event::Down:
        //printf("%d %d\n", button, flags);
        if (button <= RightButton) master->viewer->mouseState ^= (int)pow(2, (int)button);
        master->viewer->mousePress(button, true, x, y);
        break;
      case Event::Up:
        master->viewer->mouseState = 0;
        master->viewer->mousePress(button, false, x, y);
        break;
      case Event::Zoom:
        master->mouseScroll(evt.getExtraDataInt(0));
        break;
      case Event::Move:
        if (master->viewer->mouseState)
           master->viewer->mouseMove(x, y);
        break;
      default:
        printf("? %d\n", evt.getType());
    }
  }
  else if (evt.getServiceType() == Service::Keyboard)
  {
    int x = evt.getPosition().x();
    int y = evt.getPosition().y();
    int key = evt.getSourceId();
    if (evt.isKeyDown(key))
    {
      if (key > 255)
      {
      //printf("Key %d %d\n", key, evt.getFlags());
        if (key == 262) key = KEY_UP;
        else if (key == 264) key = KEY_DOWN;
        else if (key == 261) key = KEY_LEFT;
        else if (key == 263) key = KEY_RIGHT;
        else if (key == 265) key = KEY_PAGEUP;
        else if (key == 266) key = KEY_PAGEDOWN;
        else if (key == 260) key = KEY_HOME;
        else if (key == 267) key = KEY_END;
      }
      master->viewer->keyPress(key, x, y);
    }

    //Omegalib bugs: only works for left keys and alt+shift doesn't work
    master->viewer->keyState.ctrl = flags & 8;
    master->viewer->keyState.alt = flags & 16;
    master->viewer->keyState.shift = flags & 32;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FractalApplication::commitSharedData(SharedOStream& out)
{
  std::string params = master->stringify();
  out << params;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FractalApplication::updateSharedData(SharedIStream& in)
{
  std::string params;
  in >> params;
  //Store the param data on the master
  master->parse(params);
  //Update label
  myMainLabel->setText(master->description);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ApplicationBase entry point
int main(int argc, char** argv)
{
  Application<FractalApplication> app("FractalViewer");
  return omain(app, argc, argv);
}

#endif
