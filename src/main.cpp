#include "LavaVu.h"
#include "FractalVu.h"

// Main function
int main(int argc, char *argv[])
{
  FractalVu fv(GetBinaryPath(argv[0], "FractalVu"));
  std::vector<std::string> args(argv+1, argv+argc);
  fv.run(args);
  return 0;
}

