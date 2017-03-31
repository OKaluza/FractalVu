/************************************************************** 
 * Fractal Viewer in OmegaLib
 **************************************************************/

#include <omega.h>
#include <omegaGl.h>
#include <omegaToolkit.h>
#include "ViewerApp.h"
#include "../src/FractalVu.h"
#include "../src/FractalServer.h"

using namespace omega;
using namespace omegaToolkit;
using namespace omegaToolkit::ui;

class FractalApplication;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FractalRenderPass: public RenderPass
{
public:
  FractalRenderPass(Renderer* client, FractalApplication* app)
                  : RenderPass(client, "FractalRenderPass"), app(app) {}
  virtual void render(Renderer* client, const DrawContext& context);

private:
  FractalApplication* app;

};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FractalApplication: public EngineModule
{
public:
  FractalVu* glapp;
  bool isMaster;
  std::string sharedData;
  //Copy of commands
  std::deque<std::string> commands;

  FractalApplication(): EngineModule("FractalApplication")
  {
     SystemManager* sys = SystemManager::instance();
     isMaster = sys->isMaster();

  std::string expath = GetBinaryPath("FractalVR", "FractalVR");
    //Create the app
    glapp = new FractalVu(expath, true);

  //Get the executable path
    std::vector<std::string> arglist;
    //Default args
    arglist.push_back("-a");
    arglist.push_back("-S");
#ifndef DISABLE_SERVER
     //Enable server
     if (isMaster)
     {
       printf("CREATED MASTER VIEWER %p %p\n", glapp, glapp->viewer);
        arglist.push_back("-p8080");
     }
     else
       printf("CREATED VIEWER %p %p\n", glapp, glapp->viewer);
#endif

    //App specific argument processing
    glapp->run(arglist);

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
    r->addRenderPass(new FractalRenderPass(r, this));
  }

  virtual void commitSharedData(SharedOStream& out);
  virtual void updateSharedData(SharedIStream& in);

private:
  // The ui manager
  Ref<UiModule> myUiModule;
  // The root ui container
  Ref<Container> myUi;  
  //Widgets
  Ref<Label> myMainLabel;
  std::string labelText;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FractalRenderPass::render(Renderer* client, const DrawContext& context)
{
  //if(context.task == DrawContext::SceneDrawTask)
  if(context.task == DrawContext::OverlayDrawTask)
  {
    if (!app->glapp->viewer->isopen)
    {
      DisplayConfig& dcfg = SystemManager::instance()->getDisplaySystem()->getDisplayConfig();
      const Rect& vp = dcfg.getCanvasRect();
      Vector2i resolution = Vector2i(vp.width(), vp.height());
      //Setup tile render adjustments
      app->glapp->viewer->open(context.tile->pixelSize[0], context.tile->pixelSize[1]);
      app->glapp->viewer->init();
      app->glapp->dims[0] = resolution[0];
      app->glapp->dims[1] = resolution[1];
      if (context.tile->isInGrid)
      {
        app->glapp->tiles[0] = resolution[0] / context.tile->pixelSize[0];
        app->glapp->tiles[1] = resolution[1] / context.tile->pixelSize[1];
        app->glapp->tile[0] = context.tile->gridX;
        app->glapp->tile[1] = context.tile->gridY;
        std::cout << context.tile->id <<  " : Tiled config: " << app->glapp->tile[0] << "," << app->glapp->tile[1] << " of " << app->glapp->tiles[0] << "," << app->glapp->tiles[1] << std::endl;
      }
      else
      {
        std::cout << "Master window : " << context.tile->pixelSize[0] << " x " << context.tile->pixelSize[1] << std::endl;
      }
      printf("[%d tile %d] -- viewer %p glapp %p master %p\n", context.tile->isInGrid, context.tile->id, app->glapp->viewer, app->glapp, app->isMaster);

      //Set on demand rendering
      //omega::Camera* cam = Engine::instance()->getDefaultCamera();
      //cam->setMaxFps(0);
      //cam->queueFrameDraw();
    }

    //Server update?
    //printf("[%d tile %d] -- %d commands viewer %p app %p\n", context.tile->isInGrid, context.tile->id, FractalServer::data.size(), viewer, glapp);
    if (FractalServer::data.size() > 0)
    {
      app->sharedData = FractalServer::data.front();
      FractalServer::data.pop_front();
      app->glapp->update(app->sharedData);
      //printf("%d\n", FractalServer::data.size());
      //std::cout << sharedData << std::endl;
    }

    //client->getRenderer()->beginDraw3D(context);
    client->getRenderer()->beginDraw2D(context);

    app->glapp->viewer->display();
    GL_Error_Check;

    client->getRenderer()->endDraw();
    GL_Error_Check;
    app->glapp->newframe = true; //Force new frame
  }

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FractalApplication::commitSharedData(SharedOStream& out)
{
  //std::string params = master->stringify();
  std::string params;
  if (isMaster)
  {
    params = sharedData;
    sharedData = "";
  }
  out << params;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FractalApplication::updateSharedData(SharedIStream& in)
{
  std::string params;
  in >> params;
  //Store the param data on the master
  //master->parse(params);
  //Update label
  //myMainLabel->setText(master->description);
  if (!isMaster && params.length())
  {
    FractalServer::data.push_back(params);
    //omega::Camera* cam = Engine::instance()->getDefaultCamera();
    //cam->queueFrameDraw();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ApplicationBase entry point
int main(int argc, char** argv)
{
  Application<FractalApplication> app("FractalVR");
  return omain(app, argc, argv);
}

