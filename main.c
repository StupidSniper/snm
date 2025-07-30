#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/xf86vmode.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ini.h>
#include <string.h>
#include <time.h>


#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

#define COLOR_BOLD_RED "\x1b[1;31m"
#define COLOR_BOLD_CYAN "\x1b[1;36m"


typedef struct {
  int start_hour;
  int end_hour;
  float brightness;
  float shift;
  float transition_time;
} config_t;


int help() {
  printf("Usage: snm [options]\n"
         "Options: \n"
         "  -f     File path for config file (defaults to $HOME/.config/snm/snm.conf)\n"
         "  -r     Reset gamma and brightness\n"
         "  -h     Show help screen\n");

  exit(0);
}


char *readIn (FILE *file) {
  int eof;
  int insize = 0;

  if (file == stdin) {
    eof = '\n';
    insize = 8;
  }
  else if (file != NULL){
    eof = EOF;
    insize = 64;
  }

  int currInd = 0;
  int read = fgetc(file);
  char *out = malloc(insize * sizeof(char));
  if (!out) return NULL;

  while (read != eof) {
    out[currInd] = read;
    currInd++;

    if (currInd >= insize) {
      insize *= 2;
      char *tmp = realloc(out, insize * sizeof(char));

      if (!tmp) {
        free(out);
        return NULL;
      }
      out = tmp;
    }
    read = fgetc(file);
  }

  out[currInd] = '\0';
  return out;
}


int handler(void* user, const char* section, const char* name, const char* value) {
  config_t* config = (config_t*)user;

  if (strcmp(section, "time") == 0) { // time section
    if (strcmp(name, "start_hour") == 0) {
      config->start_hour = atoi(value);
    } else if (strcmp(name, "end_hour") == 0) {
      config->end_hour = atoi(value);
    } else {
      return 0; // Unknown key
    }
  } 

  else if (strcmp(section, "main") == 0) { // main section
    if (strcmp(name, "brightness") == 0) {
      config->brightness = strtof(value, NULL);
    } else if (strcmp(name, "shift") == 0) {
      config->shift = strtof(value, NULL);
    } else if (strcmp(name, "transition_time") == 0) {
      config->transition_time = strtof(value, NULL);
    } else {
      return 0;
    }
  } else {
    return 0;
  }

  return 1; // success
}


void reset() {
  Display *display = XOpenDisplay(NULL);
  if (!display) {
      fprintf(stderr, "Cannot open display\n");
      exit(0);
  }

  int screen = DefaultScreen(display);
  XF86VidModeGamma gamma;

  gamma.red = 1;
  gamma.green = 1;
  gamma.blue = 1;

  if (!XF86VidModeSetGamma(display, screen, &gamma)) {
      fprintf(stderr, "Failed to set gamma\n");
  } else {
      printf("Gamma set successfully.\n");
  }
  XCloseDisplay(display);

  FILE *fp;
  fp = popen("xrandr | grep \" connected\" | awk '{print $1}'", "r");

  if (!fp) {
    printf("No displays connected\n");
    pclose(fp);
    exit(0);
  }

  char output[64];
  while (fgets(output, sizeof(output), fp) != NULL) {
      output[strcspn(output, "\n")] = 0;

      char cmd[128];
      snprintf(cmd, sizeof(cmd), "xrandr --output %s --brightness 1", output);
      system(cmd);
  }
  printf("Brightness set successfully\n");

  pclose(fp);
  exit(0);
}


int gettime() {
  FILE *timefile;
  timefile = popen("date +%H%M", "r");

  if(!timefile) {
    printf("time fetch failed\n");
    return 0;
  }

  int time = atoi(readIn(timefile));
  pclose(timefile);
  return time;
}


int setscreen(config_t config){
  Display *display = XOpenDisplay(NULL);
  if (!display) {
      fprintf(stderr, "Cannot open display\n");
      return 1;
  }

  int screen = DefaultScreen(display);

  XF86VidModeGamma gamma;

  float normal = 1.0f;
  float night_red = 1.2f;
  float night_green = 0.9f;
  float night_blue = 0.6f;

  gamma.red   = normal * config.shift + night_red   * (1.0f - config.shift);
  gamma.green = normal * config.shift + night_green * (1.0f - config.shift);
  gamma.blue  = normal * config.shift + night_blue  * (1.0f - config.shift);


  if (!XF86VidModeSetGamma(display, screen, &gamma)) {
      fprintf(stderr, "Failed to set gamma\n");
  } else {
      printf("Gamma set successfully.\n");
  }

  XCloseDisplay(display);


  FILE *fp;
  fp = popen("xrandr | grep \" connected\" | awk '{print $1}'", "r");

  if (!fp) {
    printf("No displays connected\n");
    pclose(fp);
    return 0;
  }

  char output[64];
  while (fgets(output, sizeof(output), fp) != NULL) {
      output[strcspn(output, "\n")] = 0;

      char cmd[128];
      snprintf(cmd, sizeof(cmd), "xrandr --output %s --brightness %f", output, config.brightness);
      system(cmd);
  }
  printf("Brightness set successfully\n");

  pclose(fp);
}

int main(int argc, char *argv[]) {
  int opt;
  char *filename = NULL;

  while ((opt = getopt(argc, argv, "f:hr")) != -1) {
    switch (opt) {
      case 'f':
        filename = optarg;
        break;
      case 'r':
        reset();
        break;
      case 'h':
        help();
      default: 
        exit(0);
    }
  }

  if (!filename) {    
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "HOME not set\n");
        return 1;
    }

    const char *suffix = "/.config/snm/snm.conf";
    size_t path_len = strlen(home) + strlen(suffix) + 1; // +1 for null terminator

    filename = malloc(path_len);
    if (!filename) {
        perror("malloc failed");
        return 1;
    }

    snprintf(filename, path_len, "%s%s", home, suffix);
    printf("Config path: %s\n", filename);
  } else {
    printf("Config path: %s\n", filename);
  }


  FILE *configfile;
  configfile = fopen(filename, "r");

  if (configfile == NULL) {
    printf(COLOR_BOLD_RED "error:" COLOR_RESET " invalid file '" COLOR_BOLD_CYAN "%s" COLOR_RESET "'\n", filename);
    exit(0);
  }


  config_t config = {0};

  if (ini_parse(filename, handler, &config) < 0) {
      fprintf(stderr, "Couldn't read config file\n");
      return 1;
  }

  free(configfile);
  free(filename);

  config_t tempconf = config;
  while (1) {
    int time = gettime();
    float timeshift = time;
    printf("%f transition_time\n", config.transition_time);
    if (config.start_hour > config.end_hour) {
      if (24 - config.end_hour + config.start_hour < config.transition_time * 2) {
        printf("Cannot have transition_time * 2 be larger than the total time shift is active\n");
        exit(0);
      } else if (config.start_hour < time) {
        timeshift = time - config.start_hour;
      } else if (config.end_hour > time) {
        timeshift = config.end_hour - time;
      } else {
        timeshift = 0;
      }
    } else if (config.end_hour > config.start_hour) {
      if (config.end_hour - config.start_hour < config.transition_time * 2) {
        printf("Cannot have transition_time * 2 be larger than the total time shift is active\n");
        exit(0);
      } if (time > config.start_hour && time < config.end_hour) {
        int diststart = time - config.start_hour;
        int distend = config.end_hour - time;
        if (diststart > distend) {
          timeshift = diststart;
        } else {
          timeshift = distend;
        }
      } else {
        timeshift = 0;
      }
    } else {
      printf("Cannot have end_hour and start_hour be the same\n");
      exit(0);
    }
    if (timeshift > config.transition_time) {
      timeshift = config.transition_time;
    }
    timeshift = timeshift / config.transition_time;
    printf("%f final timeshift\n", timeshift);
    tempconf.brightness = 1 - ((1 - config.brightness) * timeshift);
    tempconf.shift = 1 - ((1 - config.shift) * timeshift);
    printf("%f final brightness %f final gamma shift\n", tempconf.brightness, tempconf.shift);
    setscreen(tempconf);
    sleep(config.transition_time);
  }
}
