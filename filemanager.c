#include <stdlib.h>
#include <stdio.h>
#include <curses.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

//gcc -lncurses filemanager.c

#define MAX_FILES 1024
#define MAX_FILENAME_LENGTH 1024
#define BUFFOR_LENGTH 1024
#define MAX_PATH_LENGTH 1024


int is_dir(const char* str1){
    int is_dir1 = (str1[0] == '/');
    if(is_dir1)
        return 1;
    else
        return -1;
}

int compare_dir(const char *str1, const char *str2){
    if(is_dir(str1) && !is_dir(str2))
        return -1; // str1 
    else if(!is_dir(str1) && is_dir(str2))
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
    }else {
        perror("dir open: 77    ");
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

off_t get_total_size(char* path){
    int fd_read = open(path, O_RDONLY);
    if(fd_read < 0){
        perror("read fd: 89");
        close(fd_read);
        return -1;
    }


    off_t total_size = 0;
    char buffor[BUFFOR_LENGTH];
    unsigned bytes_read;
    while((bytes_read = read(fd_read, buffor, BUFFOR_LENGTH)) > 0){
        total_size += bytes_read;
    }
    return total_size;
}

#define NUM_THREADS 4

typedef struct{
    char src_path[MAX_PATH_LENGTH];
    char dest_path[MAX_PATH_LENGTH];
    off_t start_offset;
    off_t end_offset;
}copy_args;

float progress = 0;
pthread_mutex_t progress_mutex = PTHREAD_MUTEX_INITIALIZER;

void* copy_file_thread(void* arg) {
    copy_args* cpyargs = (copy_args*)arg;
    char buffor[BUFFOR_LENGTH];
    ssize_t bytes_read, bytes_written;
    //read
    int fd_read = open(cpyargs->src_path, O_RDONLY);
    if(fd_read < 0){
        perror("read fd: 129");
        close(fd_read);
        return NULL;
    }
    //write + creating if dest not exist
    int fd_write = open(cpyargs->dest_path, O_WRONLY | O_CREAT, 0775);
    if(fd_write < 0){
        perror("write fd: 135");
        close(fd_write);
        return NULL;
    }

      //setting fds offset pos
    lseek(fd_read, cpyargs->start_offset, SEEK_SET);
    lseek(fd_write, cpyargs->start_offset, SEEK_SET);

    off_t bytes_to_copy = cpyargs->end_offset - cpyargs->start_offset;
    while(bytes_to_copy > 0){
        bytes_read = read(fd_read, buffor, BUFFOR_LENGTH);
        if(bytes_read < 0){
            perror("Read B: 147");
            break;
        }
        bytes_written = write(fd_write,buffor,bytes_read);
        if(bytes_written < 0){
            perror("Written B: 152");
            break;
        }

        bytes_to_copy -= bytes_written;


        // progress
        pthread_mutex_lock(&progress_mutex);
        progress += bytes_written;
        pthread_mutex_unlock(&progress_mutex);
    }
    close(fd_read);
    close(fd_write);
}

void copy_file(char* src_path, char* dest_path, char* name){
    progress = 0;
    off_t total_size = get_total_size(src_path);
    size_t total_bytes_written = 0;
    char buffor[BUFFOR_LENGTH];
    size_t bytes_read, bytes_written;


    pthread_t threads[NUM_THREADS];
    copy_args cpyargs[NUM_THREADS];

    off_t chunk_size = total_size/NUM_THREADS;
    off_t remaining_bytes= total_size%NUM_THREADS;
    off_t offset = 0;

    for(int i = 0; i<NUM_THREADS; i++){
        cpyargs[i].start_offset = offset;
        if(i==NUM_THREADS-1){
            chunk_size += remaining_bytes;
        }
        cpyargs[i].end_offset = offset + chunk_size;
       
        strcpy(cpyargs[i].dest_path, dest_path);
        strcpy(cpyargs[i].src_path, src_path);

        pthread_create(&threads[i], NULL, copy_file_thread, &cpyargs[i]);
        offset += chunk_size;
    }

    WINDOW* progress_win;
    progress_win = newwin(15,50,10,50);
    box(progress_win,0,0);
    wbkgd(progress_win, COLOR_PAIR(3));
    mvwprintw(progress_win,1,1,"Copying %s",name);

    float old_percent = -1.0;
    while(progress < (float)total_size){
        pthread_mutex_lock(&progress_mutex);
        float current_percent = progress/(float)total_size * 100.0;
        if(current_percent != old_percent){
            mvwprintw(progress_win,7,25,"%.f%%", current_percent);
            wrefresh(progress_win);
            old_percent = current_percent;
        }
        pthread_mutex_unlock(&progress_mutex);
    }

    for(int i = 0; i<NUM_THREADS; i++){
        pthread_join(threads[i],NULL);
    }
    //completion
    mvwprintw(progress_win, 2, 2, "100%% Complete");
    wrefresh(progress_win);
    usleep(500);

    werase(progress_win);
    delwin(progress_win);
}

void copy_files(char* src_path, char* dest_path, char* name){
    int is_dir = (name[0] == '/');
    char dirents[MAX_FILES][MAX_FILENAME_LENGTH];
    int dir_count = 0;
    char new_src[MAX_PATH_LENGTH];
    char new_dest[MAX_PATH_LENGTH];
    char new_name[MAX_FILENAME_LENGTH];
    if(is_dir){
        list_dirents(src_path,dirents,&dir_count);
        mkdir(dest_path,0775);
        
        for(int i = 1; i < dir_count; i++){

            strcpy(new_src, src_path);
            strcat(new_src, "/");
            strcat(new_src, dirents[i]);

            strcpy(new_dest, dest_path);
            strcat(new_dest, "/");
            strcat(new_dest, dirents[i]);

            copy_files(new_src, new_dest, dirents[i]);

        }
    }else{
        copy_file(src_path,dest_path,name);
    }

}

char* homechar(char* path);

void copy_dialog(char* src_path, char* name){
    WINDOW* win;
    win = newwin(15,50,10,50);
    box(win,0,0);
    keypad(win,TRUE);
    wbkgd(win, COLOR_PAIR(2));
    mvwprintw(win,0,0,"COPY DIALOG");
    mvwprintw(win,2,1,"EXAMPLE: ");
    mvwprintw(win,3,1,"if dir:  ~/documents/");
    mvwprintw(win,4,1,"if file: ~/pictures/");
    char* str;

    if(is_dir(name)){
        str = "Copy directory:";
    }else{
        str = "Copy file:";
    }
    mvwprintw(win,6,1,str);
    mvwprintw(win,7,1,src_path);
    mvwprintw(win,9,1,"To:");

    
    touchwin(win);
    char dest_path[MAX_PATH_LENGTH];
    char ch = wgetch(win);
    if(ch == 27)
        return;
    echo();
    
    while(1){
        memset(dest_path, '\0', MAX_PATH_LENGTH);
        mvwgetstr(win,10,1,dest_path);
        if(strlen(dest_path) > 0)
            break;
    }
    char* new_dest = homechar(dest_path);

    
    if(is_dir(name)){
        strcat(new_dest,name);
    }else{
        strcat(new_dest,"/");
        strcat(new_dest,name);
    }

    
    copy_files(src_path,new_dest,name);
    free(new_dest);
    werase(win);
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
            char new_name[FILENAME_MAX];
            strcpy(new_name,dirents[i]);
            if(new_name[0] == '/'){
                char new_src[MAX_FILENAME_LENGTH];
                strcpy(new_src,src_path);
                strcat(new_src,"/");
                strcat(new_src,dirents[i]);
                delete_files(new_src,new_name);
            }
            else{
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


char* homechar(char* path){
    char* full_path = NULL;
    if(path[0] == '~')
    {
        const char* home_path= getenv("HOME");
        size_t home_len = strlen(home_path);
        size_t path_len = strlen(path + 1); 
        
        full_path = (char*)malloc(home_len + path_len + 10);
        strcpy(full_path, home_path);
        strcat(full_path, path + 1);
    }
    else{
        full_path = (char*)malloc(strlen(path) + 10);
        strcpy(full_path, path);
    }
    return full_path;
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
    init_pair(3, COLOR_MAGENTA, COLOR_GREEN);  
    
    //fullscreen
    WINDOW* mainscreen = newwin(0,0,0,0);
    wbkgd(mainscreen, COLOR_PAIR(1));
    box(mainscreen, 0, 0);
    keypad(mainscreen, TRUE);
    nodelay(mainscreen, TRUE);
    
    
    getcwd(current_path, sizeof(current_path));
    list_dirents(current_path,dirents,&dir_count);
    mvwprintw(mainscreen,0, 0, current_path);
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
                
                if(is_dir(name)){ //check if dir
                    if(highlight != 1){
                        // strcat(current_path,"/");
                        strcat(current_path,dirents[highlight-1]);
                        chdir(current_path);
                        getcwd(current_path, sizeof(current_path));
                        //clearnig buffor
                        for(int i = 0; i < MAX_FILES; i++){
                            memset(dirents[i], '\0', MAX_FILENAME_LENGTH);
                        }
                        highlight = 1;
                        list_dirents(current_path,dirents,&dir_count);
                        werase(mainscreen);
                        box(mainscreen, 0, 0);
                        mvwprintw(mainscreen,0, 0, current_path);
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
                        mvwprintw(mainscreen,0, 0, current_path);
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
                mvwprintw(mainscreen,0, 0, current_path);
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