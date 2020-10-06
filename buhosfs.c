#include "../include/fs.h"
#include "../include/string.h"
#include "../include/fb.h"
#include "../include/mem.h"
#include "../include/ata.h"
#include "../include/disk.h"
#include "../include/fs.h"
#include "../include/interrupts.h"
#include "../include/io.h"
#include "../include/kb.h"
#include "../include/pic.h"
#include "../include/typedef.h"

/* Superblock */

#define BUHOSFS_MAGIC               0x53465f534f485542
#define sup_block_OFFSET                   1024

#define BLK_SIZE_1K                 1024
#define BLK_SIZE_2K                 2048
#define BLK_SIZE_4K                 4096

typedef struct {
  u64 magic;
  u32 block_size;
  u32 total_blocks;
  u32 free_blocks;
  u32 fat_off;
  u32 fat_size;
  u32 root_off;
  u32 root_size;
}__attribute__((__packed__)) sup_block_t;

/* FAT */

#define FIXD                      0x80000000
#define BADB                      0x40000000
#define LAST                      0x20000000
#define USED                      0x10000000
#define UNLAST                    0xdfffffff
#define UNUSED                    0xefffffff

#define TYPE_MASK                 0xf0000000
#define NEXT_BLOCK_MASK           0x0fffffff

typedef u32 fat_entry_t;

/* Direntries */

#define TYPE_EMPTY                0x00000000
#define TYPE_REGULAR_FILE         0x10000000
#define TYPE_DIRECTORY            0x20000000
#define TYPE_LAST                 0xf0000000

#define TYPE_MASK                 0xf0000000
#define OFF_MASK                  0x0fffffff

#define MAX_NAME_LEN              15

typedef struct {
  u32 type_and_off;
  u32 size;
  u32 ctime;
  u32 mtime;
  char name[MAX_NAME_LEN + 1];
}__attribute__((__packed__)) direntry_t;

/*Variables*/
sup_block_t sup_block;
int *table_fat;

char tokens[1024][MAX_NAME_LEN + 1];
int count_tokens;
char name_last_token[MAX_NAME_LEN + 1]; 
u32 type_dir;


int block_to_sector(partition_t *part, int block_index)
{

  int result = part -> lba_start + ((block_index * sup_block.block_size) / part -> sector_size);
  
  return result;
}
/*GENERALES*/
int fs_init(partition_t *part) 
{
  //cargar el super bloque y comprobar q tiene el tamaño correto 1k,2k, o 4k
  ata_read(&part->dev, (part->lba_start) + sup_block_OFFSET/(part->sector_size), 1, &sup_block);


  if(sup_block.magic != BUHOSFS_MAGIC || (sup_block.block_size!=BLK_SIZE_1K && sup_block.block_size != BLK_SIZE_2K && sup_block.block_size != BLK_SIZE_4K))
  {
      return -EINVFS;
  }

  //lleno la tabla fat
  table_fat = (int *)kalloc(sup_block.fat_size * sup_block.block_size);
  //int sector_pos = block_to_sector(part, sup_block.fat_off);
  int error;
  int j;
  int s =sup_block.fat_size;
  for (j = 0; j < s; ++j)
  {
      error = ata_read(&(part->dev),
                       block_to_sector(part, (sup_block. fat_off) +j),
                       (sup_block.block_size / part->sector_size),
                       ((char *)table_fat) + ((sup_block.block_size)*j));
      if (error < 0) {kfree(table_fat);while(1);}
  }
    /*//Comprobacion de la tabla FAT
       fb_printf("--------------------------------\n");

       int p = 0;
       int l = table_fat[162] & NEXT_BLOCK_MASK;
       for(; p < 10; p++)
       {

       fb_printf("%d\n", l);
       int next = table_fat[l] & NEXT_BLOCK_MASK;

       l = next;

       }*/

  return 0;
}

int fs_destroy(partition_t *part) 
{

  /*
  //guardamos el superbloque en disco
  ata_write(&part->dev, (part->lba_start) + sup_block_OFFSET/(part->sector_size), 1024/ part-> sector_size, sup_block);

  //guardamos la tabla fat en disco
  ata_read(&part->dev, (sup_block.fat_off * sup_block.block_size / part->sector_size) + part->lba_start, 200, table_fat);
  ata_read(&part->dev, ((sup_block.fat_off * sup_block.block_size / part->sector_size) + part->lba_start)+200, 120, table_fat + 200);
  
  */
  kfree(&sup_block);
  kfree(table_fat);
  
  return 0;
}

int fs_stats(partition_t *part, fs_stat_t *stat)
{
  if(sup_block.magic != BUHOSFS_MAGIC) return -EINVFS;

  stat->block_size = sup_block.block_size;
  stat->total_blocks = sup_block.total_blocks;
  stat->free_blocks = sup_block.free_blocks;

  return 0;
}
void LoadBlock (partition_t *part, u32 dir_off, u32 size, direntry_t *dir)
{
  ata_device_t device = part->dev;
  u32 start = part->lba_start;
  u32 sector_size = part->sector_size;
  u32 sector_pos = dir_off * sup_block.block_size/sector_size;
  u32 sector_count = sup_block.block_size/sector_size;

  ata_read(&device, start + sector_pos, sector_count, dir);
}

void strncpy(char * dest, char * src, int n){
  int i;
  for (i = 0; i < n; ++i){
    dest[i] = src[i];
  }
}

int strncmp(char * s1, char * s2){
    char a1 = *s1;
    char a2 = *s2;
    while(a1 == a2 && a1 !=0 && a2 != 0){
        s1++;
        s2++;
        a1 = *s1;
        a2 = *s2;
    }
    if(a1 == a2)return 0;
    return -1;
}

int Token(char* path)
{
    //fb_printf("path een token %s\n", path);
    int i = 0;
    int len_name = 0;

    char * path1 = path;
    /*(char *)kalloc(strlen(path));
    if(path1 == NULL) {fb_printf("dio palo en kalloc de token\n"); int d=2; while(d>0)d--;}
    int k;
    for (k = 0; k < strlen(path); ++k)
    {
      path1[k]=path[k];
    }
    path1[k]='0';*/
    char name[16];

    if(*path1 != '/')
    {
        kfree(path1);
        return -1;
    }

    path1++;

    while(*path1 != 0){
      //si encontre un / tengo q guardar el token
        if(*path1 == '/'){
            path1++;
            name[len_name] = 0;
            if(strncmp(name, ".") == 0 || strncmp(name, "..") == 0)
              {kfree(path1); 
                fb_printf("error en comparaccion\n"); 
                return -ENODIR;
              }
            len_name++;
            strcpy(tokens[i], name);
            //fb_printf(">>>>>>>>> %s\n",tokens[i]);
            i++;
            len_name = 0;
        }
        else{
          if(*path1 == '\0') {kfree(path1); fb_printf("*path1 == '\0'\n"); return -ENODIR;}
          name[len_name] = *path1;
          path1++;
          len_name++;
          //fb_printf("------- %d y path1 %s                 \n", len_name, &name[len_name-1]);
          //fb_printf("len-%d y max-%d\n", len_name, MAX_NAME_LEN);
          if(len_name > MAX_NAME_LEN){kfree(path1);fb_printf("len_name > MAX_NAME_LEN\n"); return -ENODIR;}
        }
    }
    if(len_name != 0){
        name[len_name] = 0;
            len_name++;
            strcpy(tokens[i], name);
            strcpy(name_last_token, name);
            i++;
    }
    kfree(path1);
    count_tokens=i;
    return 0;
}

int FindDirectory(partition_t *part, char *path, direntry_t *dir_res)
{
  //fb_printf("entre al FindDirectory\n");
  
  direntry_t tmp;
  tmp.size=sup_block.root_size;
  strcpy(tmp.name, "/");
  tmp.type_and_off = sup_block.root_off; 

  if(strncmp("/", path) == 0 )
  {
     dir_res->size = sup_block.root_size;
     dir_res->ctime = 0;
     dir_res->mtime = 0;
     dir_res->type_and_off = sup_block.root_off;
     //fb_printf("Es la raiz\n");
     return 0;
  }

  int res = Token(path);
  if(res < 0) {fb_printf("dio un palo en token\n"); return -ENODIR;}
  
  char * dir_tmp = (char *)kalloc(sup_block.block_size);
  if(dir_tmp == NULL){fb_printf("palo en kalloc en dir_tmp\n"); return -1;}
  u32 dir_off = sup_block.root_off & OFF_MASK;
  LoadBlock(part, dir_off , 1, (direntry_t *) dir_tmp);   
  direntry_t * dir = (direntry_t *) dir_tmp;

  
  u32 directory_count = sup_block.block_size / sizeof(direntry_t);
  
  int i=0;
  int j;
  while(1){
     for (j = 0; j < directory_count; ++j)
     {
        
        //fb_printf(">>>>>>>> %s \n", dir[j].name);

        
        if((dir[j].type_and_off & TYPE_MASK) == TYPE_LAST) 
        {   
            kfree(dir);
            kfree(dir_tmp);
            return -ENOENT;
        }
        else if((dir[j].type_and_off & TYPE_MASK) == TYPE_EMPTY)
          continue;

        tmp.size = dir[j].size;
        tmp.type_and_off = dir[j].type_and_off;
        tmp.ctime= dir[j].ctime;
        tmp.mtime= dir[j].mtime;
        strcpy(tmp.name, dir[j].name);
        //fb_printf(">>>>>>>> %s \n", tmp.name);

        //fb_printf("dir[j].name  %s\n", dir[j].name);
        if(strcmp(dir[j].name, tokens[i]) == 0)
        {
          //fb_printf("encontre al token q buscaba %s\n", dir[j].name);
         
          if((tmp.type_and_off & TYPE_MASK) == TYPE_REGULAR_FILE)
          {
            //si sigue error si no sigue devolverlo
            if(i < count_tokens - 1)
            {
              //fb_printf("es un fichero y el path sigue\n");
              kfree(dir);
              kfree(dir_tmp);
              return -ENOENT;
            }

            //fb_printf("es un fichero y lo devuelvo\n");
            kfree(dir);
            kfree(dir_tmp);

            dir_res->size = tmp.size;
            dir_res->ctime = tmp.ctime;
            dir_res->mtime = tmp.mtime;
            dir_res->type_and_off= tmp.type_and_off;
            strcpy(dir_res->name, tmp.name);

            return 0;
          }
          if((tmp.type_and_off & TYPE_MASK) == TYPE_DIRECTORY)
          {
            //si no sigue el path devolverlo sino cargarlo
            if(i == count_tokens -1)
            {
              kfree(dir);
              kfree(dir_tmp);

              //fb_printf("es un directorio y lo devuelvo\n");
              dir_res->size = tmp.size;
              dir_res->ctime = tmp.ctime;
              dir_res->mtime = tmp.mtime;
              dir_res->type_and_off= tmp.type_and_off;
              strcpy(dir_res->name, tmp.name);

              return 0;
            }
            else
            {
              if(tmp.size > 0)
              {
                //fb_printf("es un directorio y tengo q seguir el path\n");
                
                kfree(dir);
                dir = (direntry_t *)kalloc(sup_block.block_size);                              
                dir_off = (tmp.type_and_off) & OFF_MASK; 
                LoadBlock(part, dir_off, 1, dir); 
                j=-1;
                i++;
                continue;
              }
              else
              {
                //fb_printf("es un directorio pero tiene size <=0\n");
                
                kfree(dir);
                kfree(dir_tmp);
                return -ENOENT;
              }
            }
          }
        }          
     }
     //si no es el ultimo bloque entonces cargo el proximo en la tabla fat
     if((table_fat[dir_off] & LAST) == 0)
     {
        //fb_printf("tengo q krgar el proximo bloque\n");
        
        kfree(dir);
        dir = (direntry_t *)kalloc(sup_block.block_size);
        if(dir == NULL) {fb_printf("error en kalloc de dir\n"); while(1);}
        int next = table_fat[dir_off] & NEXT_BLOCK_MASK;
        dir_off = next;

        LoadBlock(part, dir_off, 1, dir);
        //fb_printf("cargue el bloque\n");
     }
     else
     {
      //fb_printf("estoy en l ultimo bloque\n");
      kfree(dir);
      kfree(dir_tmp);
      return -ENOENT;
     }
  }
}

int fs_getattr(partition_t *part, char *path, stat_t *stat)
{
    direntry_t *dir_res = (direntry_t *)kalloc(sizeof(direntry_t));
    int dir = FindDirectory(part,path,dir_res); 
    if(dir < 0) {kfree(dir_res);return dir;}
    //fb_printf(" name n getattr: %s\n",dir_res->name);

    stat->size = dir_res->size;
    //fb_printf("size : %d \n", stat->size);
    stat->block_size = sup_block.block_size;
    stat->ctime = dir_res->ctime;
    stat->mtime = dir_res->mtime;
    stat->type = (dir_res->type_and_off & TYPE_MASK) >> 28;
    //fb_printf("type : %d \n", stat->type);
    //fb_printf("path---%s\n", path);
    kfree(dir_res);
    return 0;
}

int fs_open(partition_t *part, char *path, int flags)
{
  direntry_t *dir = (direntry_t *)kalloc(sizeof(direntry_t));
  int res = FindDirectory(part,path,dir); 
  if(res < 0){kfree(dir); return res;}
  //si encuentra el fichero 
  return 0;
}

int fs_read(partition_t *part, char *path, u32 offset, u32 length, void *buf)
{
  direntry_t *dir_res = (direntry_t *)kalloc(sizeof(direntry_t));
  if(dir_res== NULL){fb_printf("kalloc mal\n");  while(1);}
  int dir = FindDirectory(part,path,dir_res); 

  //fb_printf("dir----%d\n", dir);

  if(dir < 0) { fb_printf("dio palo el FindDirectory con %s %d\n", path, dir); kfree(dir_res); return dir;}

  if(dir == 0)
  {
    //fb_printf("entre aki\n");
    if((dir_res->type_and_off & TYPE_MASK) != TYPE_REGULAR_FILE)
    {
      //fb_printf("no es un TYPE_REGULAR_FILE\n");
      kfree(dir_res);
      return -ENOENT;
    }
    //verifico si el offset es mayor que el tamaño del archivo
    char* buf_tmp = (char*)kalloc(sup_block.block_size);

    if(offset >= dir_res->size)
    {
        //fb_printf("offset es mayor que el tamaño del archivo y esta OK\n");
        kfree(dir_res);
        kfree(buf_tmp);
        return 0;
    }

    u32 dir_off = (dir_res->type_and_off) & OFF_MASK;
    int sector_pos = block_to_sector(part, dir_off);
    int error = ata_read(&(part->dev),sector_pos,sup_block.block_size / part->sector_size,buf_tmp);

    if(error < 0)
    {
      //fb_printf("erro en ata_read\n");
      kfree(dir_res);
      kfree(buf_tmp);
      return -EIO;
    }

    int i = offset;
    int j = 0;
    int result = 0;
   //voy a buscar el bloque a partir del q voy a empezar a cargar
    while(i >= sup_block.block_size)
    { 
      //si no es el ultimo bloque me muevo para el proximo
      if((table_fat[dir_off] & LAST) == 0)
      {
         //fb_printf("moviendome x tablafat\n");
          dir_off = (table_fat[dir_off]) & NEXT_BLOCK_MASK;
          i-=sup_block.block_size;
      }
      else
      {
        kfree(dir_res);
        kfree(buf_tmp);
        return 0;
      }
    }

    sector_pos = block_to_sector(part,dir_off);
    error = ata_read(&(part->dev),sector_pos, sup_block.block_size / part->sector_size, buf_tmp);
    if(error < 0)
    {
      kfree(dir_res);
      return -EIO;
    }

    while(1)
    {
      for(; i < sup_block.block_size; i++)
      {
        result++;

        ((char*)buf)[j] = buf_tmp[i];
        j++;
        if((result == length) || (result + offset >= dir_res->size))
        {
          kfree(dir_res);
          kfree(buf_tmp);
          return result;
        }
      }
      //si no lo he leido todo cargo de la tabla fat el proximo bloque
      if(result < length)
      {
        if((table_fat[dir_off] & LAST) == 0)
        {
          dir_off = (table_fat[dir_off]) & NEXT_BLOCK_MASK;
          dir_off = (dir_off & OFF_MASK);
          sector_pos = block_to_sector(part,dir_off);
          ata_read(&(part->dev),sector_pos, sup_block.block_size / part->sector_size, buf_tmp);
          i = 0;
        }
        else
        { 
          kfree(dir_res);
          kfree(buf_tmp);
          return 0;
        }
      }
    }
  }
  kfree(dir_res);
  return -ENOFILE;
}

int fs_close(partition_t *part, char *path)
{
  direntry_t *dir_res = (direntry_t *)kalloc(sizeof(direntry_t));

  int dir = FindDirectory(part,path,dir_res); 
  if(dir < 0) {kfree(dir_res);return dir;}
  if(dir == 0) {kfree(dir_res);  return 0;}
}

int fs_readdir(partition_t *part, char *path, int offset, char *name)
{
    //fb_printf("estoy dentro del fs_readdir %s                   \n",path);
    //Cargar el path solicitado en dir_res
    direntry_t *dir_res = (direntry_t*)kalloc(sizeof(direntry_t));
    int dir = FindDirectory(part,path, dir_res);

    //Devolver el error guardado en dir en caso de que sea < 0
    if(dir < 0) {kfree(dir_res); fb_printf("paso algo en FindDirectory\n"); return dir;}
    
    if(dir_res->size == 0) {kfree(dir_res);return 0;}
    //Si no es una carpeta 
    if((dir_res->type_and_off & TYPE_MASK) != TYPE_DIRECTORY)
    {
      kfree(dir_res);
      return -EINVOP;
    }

    //Cargar el primer bloque de los datos de dir_res
    char * dir_tmp = (char *)kalloc(sup_block.block_size);
    u32 dir_off = dir_res->type_and_off & OFF_MASK;
    LoadBlock(part, dir_off , 1, dir_tmp);
    direntry_t * dirs = (direntry_t *) dir_tmp;

    //Calculo el maximo valor al recorrer el bloque cargado
    u32 directory_count = sup_block.block_size / sizeof(direntry_t);

    int pos;

    //Recorro el bloque que cargue arriba
    for(pos = 0 ; pos < directory_count; pos++)
    {
        // fb_printf(">> %s                                                \n", dirs[pos].name);
        //Segun el PDF, solo son validas las entradas no vacias
        if(((dirs[pos].type_and_off & TYPE_MASK) != TYPE_EMPTY))
        {
            //Si es la ultima entrada, tampoco es valida y retorno fallido
            if((dirs[pos].type_and_off & TYPE_MASK) == TYPE_LAST)
               {
                kfree(dirs);
                kfree(dir_res);
                kfree(dir_tmp);
                return 0;
               }

            //Si encontre el offset solicitado, copio para name el nombre del archivo y retorno correctamente
            if(offset == 0)
            {
                strncpy(name, dirs[pos].name, strlen(dirs[pos].name) + 1);
                kfree(dirs);
                kfree(dir_tmp);
                kfree(dir_res);
                return 1;
            }
            //Si no era vacio ni last, entonces disminuyo en 1 offset, pues el archivo era valido
            offset--;
        }       

        //Si se acabaron los archivo pero la el directorio original tiene mas bloques, cargo el next block
        if((table_fat[dir_off] & LAST) == 0 && pos == directory_count-1)
        {
              dir_off = (table_fat[dir_off]) & NEXT_BLOCK_MASK;
              kfree(dir_tmp);
              dir_tmp = (char*)kalloc(sup_block.block_size);
              kfree(dirs);
              LoadBlock(part, dir_off , 1, dir_tmp);
              dirs = (direntry_t *) dir_tmp;
              pos = -1;
              continue;
        }
    }

    kfree(dirs);
    kfree(dir_tmp);
    kfree(dir_res);
    //Si no retorne arriba, no encontre el archivo
    return 0;
}
