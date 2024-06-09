#include <stdlib.h>
#include <stdio.h>
#include <curses.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
//gcc -lncurses filemanager.c

#define MAX_FILES 1000
#define MAX_FILENAME_LENGTH 100

int compare_dir(const char *str1, const char *str2){
    int is_dir1 = (str1[0] == '/');
    int is_dir2 = (str2[0] == '/');

    if(is_dir1 && !is_dir2)
        return -1; // str1 
    else if(!is_dir1 && is_dir2)
        return 1; // str2
    else 
        return strcmp(str1, str2); //alphabetical -> /.. is first
    
}

void bubble_sort(char arr[MAX_FILES][MAX_FILENAME_LENGTH], int n, int (*compare)(const char *, const char *)){
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (compare(arr[j], arr[j + 1]) > 0) {
                char temp[MAX_FILENAME_LENGTH];
                strcpy(temp, arr[j]);
                strcpy(arr[j], arr[j + 1]);
                strcpy(arr[j + 1], temp);
            }
        }
    }
}
void list_dirents(const char* path, char dirents[MAX_FILES][MAX_FILENAME_LENGTH], int *count){
    DIR *dir;
    struct dirent *entry;
    *count = 0;

    if((dir = opendir(path)) != NULL){
        while((entry = readdir(dir)) != NULL && *count < MAX_FILES){
            if (strcmp(entry->d_name, ".") == 0){
                continue;
            }

            if(entry->d_type == DT_DIR){
                char temp[100];
                //clearowanie buffora
                temp[0] = '\0';
                strcat(temp,"/");
                strcat(temp,entry->d_name);
                strcpy(dirents[*count],temp);
            }else{
                strcpy(dirents[*count],entry->d_name);
            }
            
            *count = *count + 1;
        }
        closedir(dir);
    }
    else{
        clear();
        refresh();
        mvprintw(25, 25,"to nie folder");
    }   
    bubble_sort(dirents, *count, compare_dir);
}


void display_dirents(const char* path, char dirents[MAX_FILES][MAX_FILENAME_LENGTH],int count, int highlight){
    for(int i = 0; i < count; i++){
        if(highlight == i + 1) //present
        {
            attron(A_REVERSE);
            mvprintw(i+1, 0,"%s", dirents[i]);
            attroff(A_REVERSE);
        }
        else
            mvprintw(i+1, 0,"%s", dirents[i]);
    }
}

int main() {
    char dirents[MAX_FILES][MAX_FILENAME_LENGTH];
    int dir_count = 0;
    char current_path[1000];


    // window

    int nlines = 20;
    int ncols = 20;
    int y0 = 10;
    int x0 = 10;
    WINDOW* win;

    //initialize ncurses  
    
    initscr();
    noecho();
    cbreak();


    win = newwin(nlines,ncols,y0,x0);
    keypad(win, TRUE);
    mvprintw(0, 0, "MENU");

    getcwd(current_path, sizeof(current_path));
    list_dirents(current_path,dirents,&dir_count);

    //listing and moving 
    int ch;
    int highlight = 1;
    display_dirents(current_path, dirents, dir_count, highlight);
    
    refresh();
    while(1)
    {
        ch = wgetch(win);
        switch(ch){
            case KEY_UP:
                if(highlight == 1)
                    highlight = dir_count; //goes to last 
                else
                    highlight--;
                break;
            case KEY_DOWN:
                if(highlight == dir_count)
                    highlight = 1; //return to first 
                else
                    highlight++;
                break;
            case KEY_RIGHT:
                char* name = dirents[highlight-1];
                int is_dir = (name[0] == '/');

                if(is_dir){ //check if dir
                    if(highlight != 1){
                        strcat(current_path,"/");
                        strcat(current_path,dirents[highlight-1]);
                        chdir(current_path);
                        //clearnig buffor
                        for(int i = 0; i < MAX_FILES; i++){
                            memset(dirents[i], '\0', MAX_FILENAME_LENGTH);
                        }
                        highlight = 1;
                        list_dirents(current_path,dirents,&dir_count);
                        refresh();
                        clear();
                        display_dirents(current_path, dirents, dir_count, highlight);
                    }else{
                        highlight = 1;
                        chdir("..");
                        getcwd(current_path, sizeof(current_path));
                        list_dirents(current_path,dirents,&dir_count);
                        refresh();
                        clear();
                        display_dirents(current_path, dirents, dir_count, highlight);
                        
                    }
                }else{
                    // clear();
                    // mvprintw(5, 10, "Plik");
                    // refresh();
                }
                break;
            case KEY_LEFT:
                highlight = 1;
                chdir("..");
                getcwd(current_path, sizeof(current_path));
                list_dirents(current_path,dirents,&dir_count);
                refresh();
                clear();
                display_dirents(current_path, dirents, dir_count, highlight);
                break;
            case KEY_F(1):
                
                break;
            default:
                refresh();
                break;
        }

        if(ch == 'q')
            break;

        display_dirents(current_path, dirents, dir_count, highlight);
        refresh();
    }

    endwin();
    return 0;
}