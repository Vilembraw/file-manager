#include <stdlib.h>
#include <stdio.h>
#include <curses.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>


//gcc -lncurses filemanager.c

#define MAX_FILES 1000

void list_dirents(const char* path, char dirents[MAX_FILES][100], int *count){
    DIR *dir;
    struct dirent *entry;
    *count = 0;
    
    if((dir = opendir(path)) != NULL){
        while((entry = readdir(dir)) != NULL && *count < MAX_FILES){
            strcpy(dirents[*count],entry->d_name);
            *count = *count + 1;
        }
        closedir(dir);
    }

}

int main() {
    char dirents[MAX_FILES][100];
    int dir_count = 0;
    char current_path[100];


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
    refresh();
   
    //listing and moving 
    int ch;
    while(1)
    {
        ch = wgetch(win);
        switch(ch){
            default:
            mvprintw(5, 5,"%c", (char)ch);
            refresh();
			break;
        }

        if(ch == 'q')
            break;
    }

    endwin();
    return 0;
}