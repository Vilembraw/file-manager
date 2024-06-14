#include <stdlib.h>
#include <stdio.h>
#include <curses.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
//gcc -lncurses filemanager.c

#define MAX_FILES 1000
#define MAX_FILENAME_LENGTH 100
#define BUFFOR_LENGTH 5000
#define MAX_PATH_LENGTH 1000

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
                temp[0] = '\0';
                strcat(temp,"/");
                strcat(temp,entry->d_name);
                strcpy(dirents[*count],temp);
            }
            else{
                strcpy(dirents[*count],entry->d_name);
            }

            *count = *count + 1;
        }
        closedir(dir);
    }   
    bubble_sort(dirents, *count, compare_dir);
}


void display_dirents(const char* path, char dirents[MAX_FILES][MAX_FILENAME_LENGTH],int count, int highlight, WINDOW* mainscreen){
    for(int i = 0; i < count; i++){
        if(highlight == i + 1) //present
        {
            wattron(mainscreen,A_REVERSE);
            mvwprintw(mainscreen, i+1, 1,"%s", dirents[i]);
            wattroff(mainscreen,A_REVERSE);
        }
        else
            mvwprintw(mainscreen, i+1, 1,"%s", dirents[i]);
    }
    wrefresh(mainscreen);
}


int copy_file(char* src_path, char* dest_path){
    //read
    int fd_read = open(src_path, O_RDONLY);
    if(fd_read < 0){
        perror("open");
        close(fd_read);
        return -1;
    }
    //write + creating if dest not exist
    int fd_write = open(dest_path, O_WRONLY | O_CREAT, 0775);
    if(fd_write < 0){
        perror(dest_path);
        close(fd_write);
        return -1;
    }

    char buffor[BUFFOR_LENGTH];
    unsigned bytes_read, bytes_written;
    while((bytes_read = read(fd_read, buffor, BUFFOR_LENGTH)) > 0){
        bytes_written = write(fd_write, buffor, bytes_read);
        memset(buffor,'\0', BUFFOR_LENGTH);

        if (bytes_written != bytes_read) {
            // perror("write b");
            perror(dest_path);
            close(fd_read);
            close(fd_write);
            return -1;
        }
    }

    if (bytes_read < 0) {
        perror("read");
        close(fd_read);
        close(fd_write);
        return -1;
    }

    close(fd_read);
    close(fd_write);
}

void copy_dialog(char* src_path, char* name){
    // window
    int nlines = 25;
    int ncols = 50;
    int y0 = 10;
    int x0 = 10;
    WINDOW* win;
    win = newwin(nlines,ncols,y0,x0);
    box(win,0,0);
    wbkgd(win, COLOR_PAIR(2));
    wprintw(win,"TO WHERE?");
    echo();
    touchwin(win);
    char dest_path[MAX_PATH_LENGTH];
    char dirents[MAX_FILES][MAX_FILENAME_LENGTH];
    char new_dest[MAX_PATH_LENGTH];
    char new_src[MAX_PATH_LENGTH];
    int dir_count = 0;
    
    list_dirents(src_path,dirents,&dir_count);
    while(1){
        memset(dest_path, '\0', MAX_PATH_LENGTH);
        mvwgetstr(win,10,10,dest_path); //have to be full path for now
        if(strlen(dest_path) > 0)
            break;
    }
    
    mvwprintw(win,5,5,dest_path);
    wgetch(win);
    
    int is_dir = (name[0] == '/');
    if(is_dir){
        for(int i = 1; i < dir_count; i++){
            memset(new_dest,'\0', MAX_PATH_LENGTH);
            strcpy(new_dest,dest_path);
            strcat(new_dest,name);
            mkdir(new_dest, 0775);
            strcat(new_dest,"/");
            strcat(new_dest,dirents[i]);

            strcpy(new_src,src_path);
            strcat(new_src,"/");
            strcat(new_src,dirents[i]);
            copy_file(new_src,new_dest);
        }
    }
    else{
        copy_file(src_path,dest_path);
    }
    
    
    werase(win);
    wrefresh(win);
    delwin(win);
}

int delete_files(char* src_path, char* name){
    int is_dir = (name[0] == '/');
    char dirents[MAX_FILES][MAX_FILENAME_LENGTH];
    int dir_count = 0;
    char new_src[MAX_PATH_LENGTH];
    if(is_dir){
        list_dirents(src_path,dirents,&dir_count);
        for(int i = 1; i < dir_count; i++){
            memset(new_src,'\0',MAX_PATH_LENGTH);
            strcpy(new_src,src_path);
            strcat(new_src,"/");
            strcat(new_src,dirents[i]);
            if(remove(new_src) == 0){
                
            }
            else{
                perror("delete file");
                return -1;
            }
        }

        if(rmdir(src_path) == 0){
            //success
        }
        else{
            perror("delete dir");
            return -1;
        }
        
    }else{
        if(remove(src_path) == 0){
            //success dialog?
        } 
        else{
            perror("delete file");
            return -1;
        }
    }

}

void move_files(char* src_path, char* name){
    copy_dialog(src_path,name);
    delete_files(src_path,name);
}



int main() {
    char dirents[MAX_FILES][MAX_FILENAME_LENGTH];
    int dir_count = 0;
    char current_path[MAX_PATH_LENGTH];



    //initialize ncurses  
    
    initscr();
    noecho();
    cbreak();
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_WHITE, COLOR_RED);  
    
    //fullscreen
    WINDOW* mainscreen = newwin(0,0,0,0);
    wbkgd(mainscreen, COLOR_PAIR(1));
    box(mainscreen, 0, 0);
    keypad(mainscreen, TRUE);
    nodelay(mainscreen, TRUE);
    mvwprintw(mainscreen,0, 0, "MENU");
    
    getcwd(current_path, sizeof(current_path));
    list_dirents(current_path,dirents,&dir_count);

    int ch;
    int highlight = 1;
    display_dirents(current_path, dirents, dir_count, highlight, mainscreen);
    char src_path[MAX_PATH_LENGTH];
    while(1)
    {
        
        touchwin(mainscreen);
        ch = wgetch(mainscreen);
        switch(ch){
            case KEY_UP:
                //moving up
                if(highlight == 1)
                    highlight = dir_count; //goes to last 
                else
                    highlight--;
                break;
            case KEY_DOWN:
                //moving down
                if(highlight == dir_count)
                    highlight = 1; //return to first 
                else
                    highlight++;
                break;
            case KEY_RIGHT:
                //moving forward
                //selected option = highlight - 1 
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
                        werase(mainscreen);
                        box(mainscreen, 0, 0);
                        display_dirents(current_path, dirents, dir_count, highlight, mainscreen);
                    }else{
                        //moving backward on /..
                        highlight = 1;
                        chdir("..");
                        getcwd(current_path, sizeof(current_path));
                        list_dirents(current_path,dirents,&dir_count);
                        werase(mainscreen);
                        box(mainscreen, 0, 0);
                        display_dirents(current_path, dirents, dir_count, highlight, mainscreen);
                        
                    }
                }
                break;
            case KEY_LEFT:
                //moving backward
                highlight = 1;
                chdir("..");
                getcwd(current_path, sizeof(current_path));
                list_dirents(current_path,dirents,&dir_count);
                werase(mainscreen);
                box(mainscreen, 0, 0);
                display_dirents(current_path, dirents, dir_count, highlight, mainscreen);
                break;
            case KEY_F(1):
                //copying
                memset(src_path,'\0',MAX_PATH_LENGTH);
                strcpy(src_path,current_path);
                strcat(src_path,"/");
                strcat(src_path,dirents[highlight-1]);
                copy_dialog(src_path,dirents[highlight-1]);
                noecho();
                break;
            case KEY_F(2):
                //move
                memset(src_path,'\0',MAX_PATH_LENGTH);
                strcpy(src_path,current_path);
                strcat(src_path,"/");
                strcat(src_path,dirents[highlight-1]);
                move_files(src_path,dirents[highlight-1]);
                noecho();
                list_dirents(current_path,dirents,&dir_count);
                werase(mainscreen);
                box(mainscreen, 0, 0);
                display_dirents(current_path, dirents, dir_count, highlight, mainscreen);
                highlight--;
                break;
            case KEY_F(3):
                //delete
                memset(src_path,'\0',MAX_PATH_LENGTH);
                strcpy(src_path,current_path);
                strcat(src_path,"/");
                strcat(src_path,dirents[highlight-1]);
                delete_files(src_path,dirents[highlight-1]);
                list_dirents(current_path,dirents,&dir_count);
                werase(mainscreen);
                box(mainscreen, 0, 0);
                display_dirents(current_path, dirents, dir_count, highlight, mainscreen);
                highlight--;
                break;
        }

        if(ch == 'q')
            break;

        display_dirents(current_path, dirents, dir_count, highlight, mainscreen);
    }
    endwin();
    return 0;
}
