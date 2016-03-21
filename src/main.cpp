#include "LavaVu.h"
#include "FractalVu.h"

// Main function
int main(int argc, char *argv[])
{
  ViewerApp* app = new FractalVu(argv[0]);
  execute(argc, argv, app);
  delete app;
  return 0;
}

