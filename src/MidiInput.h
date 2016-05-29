#ifndef MidiInput__
#define MidiInput__

#include "InputInterface.h"

#ifdef USE_MIDI
#include <sys/soundcard.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#define  MIDI_DEVICE  "/dev/snd/midiC2D0"

//Interface for MIDI devices
class MidiInput : public InputInterface
{
  FILE* midifp = NULL;
 public:
  MidiInput()
  {
    //Open the device for reading.
    midifp = fopen(MIDI_DEVICE, "r");
    if (!midifp)
      debug_print("Error: cannot open %s\n", MIDI_DEVICE);
  }

  virtual bool get(std::string& data)
  {
    bool parsed = false;
    if (midifp)
    {
      int seqfd = fileno(midifp);
      struct pollfd fds;
      fds.fd = seqfd;
      fds.events = POLLIN;

      unsigned char inpacket[4];
      //if (read(seqfd, &inpacket, sizeof(inpacket)) > 2)
      if (poll(&fds, 1, 0) > 0)
      {
        int ret = read(seqfd, &inpacket, sizeof(inpacket));
        if (ret >= 3)
        {
          // print the MIDI byte if this input packet contains one
          switch (inpacket[0])
          {
            case 0xB0:
              /* Control/mode change */
              //printf("Control change %d = %d (%f)\n", inpacket[1], inpacket[2], (inpacket[2] / 127.0));
              //Some hard coded control mappings for testing...
              {
                std::stringstream sscmd;
                if (inpacket[1] == 41)
                  sscmd << "translatex " << (inpacket[2] / 127000.0);
                if (inpacket[1] == 42)
                  sscmd << "translatex " << (-inpacket[2] / 127000.0);
                if (inpacket[1] == 43)
                  sscmd << "translatey " << (inpacket[2] / 127000.0);
                if (inpacket[1] == 44)
                  sscmd << "translatey " << (-inpacket[2] / 127000.0);

                std::string cmd = sscmd.str();
                if (cmd.size() > 0)
                {
                  data = cmd;
                  parsed = true;
                  std::cerr << cmd << std::endl;
                }
              }
              break;
            //default:
            //  printf("received bytes: %x %x %x\n", inpacket[0], inpacket[1], inpacket[2]);
          }
        } //else 
        //printf("%d\n", ret);
      }
    }
    return parsed;
  }
};

#endif
#endif //MidiInput__



