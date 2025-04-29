#include <ctype.h>
#include <stdlib.h>

#include "parser.h"
#include "dv_image_cache.h"

#include "stb_ds.h"

static struct { char *key; scene_t *value; } *scene_index = NULL;

#define BUFFER_SIZE 2048

void clean_string(char *target_string);

static char read_buffer[BUFFER_SIZE];
static char write_buffer[BUFFER_SIZE];
static const char *std_line_read_format = "%s %n";

static int PARSE_COMPLETE = 0;

typedef enum {
  NONE,
  PROSE,
  OTHER
} text_dest_t;

scene_t *get_scene(const char *key){
  if(scene_index == NULL){
    sh_new_arena(scene_index);
  }

  scene_t *scene = shget(scene_index, key);

  if(scene == NULL){
    printf("Creating scene: [%s]\n", key);
    scene = calloc(1, sizeof(scene_t));
    strcpy(scene->title, key);
    strcpy(scene->prose, key);
    for(int i=0;i<6;i++){
      strcpy(scene->options[i].label, "unset");
    }
    shput(scene_index, key, scene);
  }

  return scene;
}

void parse_file(const char *fn){

  FILE *fp = fopen(fn, "r");
  text_dest_t text_dest = NONE;

  scene_t *selected_scene = NULL;
  uint32_t opt_n = 0;

  char command[32] = "#\0";
  char idstr[128] = "\0";
  char fname[128] = "\0";
  char text[2048] = "\0";
  uint32_t buffer_pos = 0;
  uint32_t args_pos = 0;

  while(1){
    sprintf(command,"#");
    sprintf(idstr,"");
    sprintf(text,"");

    if(!fgets( read_buffer, 2048, fp)){ break; }

    sscanf(read_buffer, "%s %n", command, &buffer_pos);
    printf("Command: %s\n", command);

    if(strcmp(command, "#") == 0){
      continue;
    }else if(strcmp(command, "SCENE") == 0){
      sscanf(&read_buffer[buffer_pos], "%s", idstr);
      //printf("Arg1: [%s]\n", idstr);
      selected_scene = get_scene(idstr);
      //strcpy(selected_scene->title, idstr);
      //strcpy(selected_scene->prose, idstr);
      opt_n = scene_option_count(selected_scene);
      continue;
    }else if(strcmp(command, "OPTION") == 0){
      if(selected_scene != NULL){
        sscanf(&read_buffer[buffer_pos], "%s %n", idstr, &args_pos);
        strcpy(selected_scene->options[opt_n].target, idstr);
        strcpy(selected_scene->options[opt_n].label, &read_buffer[buffer_pos + args_pos]);
        selected_scene->options[opt_n].enabled = 1;
        //printf("Setting opt label: %s\n", selected_scene->options[opt_n].label);
        opt_n += 1;
      }
    }else if(strcmp(command, "TITLE") == 0){
      strcpy(selected_scene->title, &read_buffer[buffer_pos]);
      clean_string(selected_scene->title);
      //printf("Stored Title: [%s]\n", &selected_scene->title);
    }else if(strcmp(command, "BGIMG") == 0){
      sscanf(&read_buffer[buffer_pos], "%s", fname);
      printf("%s\n",fname);
      selected_scene->bgimg = get_image(fname);
    }else if(strcmp(command, "PROSE") == 0){
      if(selected_scene != NULL){
        text_dest = PROSE;
        selected_scene->prose[0] = '\0';
      }
    }else if(strcmp(command, "|") == 0){
      if(text_dest == PROSE && selected_scene != NULL){
        strcat(selected_scene->prose, &read_buffer[buffer_pos]);
        clean_string(selected_scene->prose);
      }
    }else{
      printf("WARNING: Unhandled line: %s\n", read_buffer);
      continue;
    }
    sprintf(command, "");
  }
  PARSE_COMPLETE = 1;
  return;
}

scene_t *get_scene_by_idstr(const char *idstr){
  if(!PARSE_COMPLETE){ printf("Getting scene: [%s] \n", idstr); }
  scene_t *s = get_scene(idstr);
  return s;
}

void clean_string(char *target_string){
  int len = strlen(target_string);

  char copy_string[2048];

  int i=0;
  while(i<len){
    char c = target_string[i];

    if(isalnum(c) || ispunct(c)){
      // do nothing with alphanumeric and punctuation characters
    }else if(isspace(c)){
      // replace all whitespace with spaces
      c = ' ';
    }else{
      // replace anything else with underscore
      c = '_';
    }

    copy_string[i] = c;
    i++;
  }

  copy_string[i] = '\0';

  int ti=0; i=0;
  len = strlen(copy_string);
  while(i<len){
    char c = copy_string[i];

    target_string[ti] = c;
    ti += 1;
    i += 1;
  }

  target_string[ti] = '\0';

//  ti -= 1;
//  if(isspace(target_string[ti])){
//    target_string[ti] = '\0';
//  }

  return;
}

int scene_option_count(const scene_t *s){
	int count = 0;
	for(int i=0; i<6; i++){
		if(s->options[i].enabled){
			count += 1;
		}else{
			break;
		}
	}
	return count;
}





