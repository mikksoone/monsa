// monsa.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdlib.h>
#include <string.h>

#define INT int
int FILE_err=0;
INT *item_order=NULL, *item_perm=NULL, **org_frq, nRow=0, nCol=0;

typedef struct {
  INT **trsact, *buf;   // transactions, its buffer, and their sizes
  INT trsact_num, item_max, eles, max;  // #transactions, #items, total #elements
  INT ***occ, **occ_buf, **occt;  // occurrences, its buffer, and their sizes
	INT ***occt_list;
	INT ****occ_list;
	INT *skip;
  INT *frq;    // array for frequency of each item, and those in the original database
  INT *itemset, itemset_siz;  // itemset array and its size
  INT **jump, *jump_buf, jump_siz; // candidate queue and its size
  INT sigma; // minimum support
} TRSACT;

/* allocate memory with error routine */
char *alloc_memory (size_t siz){
  char *p = (char *)calloc (siz, 1);
  if ( p == NULL ){
    printf ("out of memory\n");
    exit (1);
  }
  return (p);
}

/* re-allocate memory and fill the newly allocated part by 0, with error routine */
char *realloc_memory (char *p, int sizof, size_t siz, size_t *org_siz){
  size_t i;
  p = (char *)realloc(p, siz*sizof);
  if ( p == NULL ){
    printf ("out of memory\n");
    exit (1);
  }
  for ( i=(*org_siz)*sizof ; i<siz*sizof ; i++ ) p[i] = 0;
  *org_siz = siz;
  return (p);
}
/*
int qsort_cmp_idx (const void *x, const void *y){
  if ( item_order[*((INT *)x)] < item_order[*((INT *)y)] ) return (-1);
  else return ( item_order[*((INT *)x)] > item_order[*((INT *)y)] );
}
*/
/* read an integer from the file */
INT FILE_read_int (FILE *fp){
  INT item;
  int flag =1;
  int ch;
  FILE_err = 0;
  do {
    ch = fgetc (fp);

    if ( ch == '\n' ){ FILE_err = 5; return (0); }
    if ( ch < 0 ){ FILE_err = 6; return (0); }
    if ( ch=='-' ) flag = -1;
  } while ( ch<'0' || ch>'9' );
  for ( item=(int)(ch-'0') ; 1 ; item=item*10 +(int)(ch-'0') ){
    ch = fgetc (fp);
    if ( ch == '\n' ){ FILE_err = 1; return (flag*item); }
    if ( ch < 0 ){ FILE_err = 2; return (flag*item); }
    if ( (ch < '0') || (ch > '9')) return (flag*item);
  }
}

void TRSACT_print (TRSACT *T, INT *occ, INT frq, INT end){
  INT i, j, *tr, item, tID;
  printf ("------------\n");
  for ( i=0 ; i<frq ; i++ ){
    tID = occ[i];
    tr = T->trsact[tID];
    for ( j=0 ; (item=tr[j]) != end ; j++ ) printf ("%d ", item);
    printf ("\n");
  }
  printf ("============\n");
}

void print_int_arr(INT *arr, INT len, char *name){
	INT i;
	//return;
	for( i=0 ; i<len ; i++){
		printf("%d ", arr[i]);
	}
	printf ("==%s==========\n", name);
}
/* load a transaction from the input file to memory */
void TRSACT_file_load (TRSACT *T, char *fname){
  INT item, t, i, j;
  size_t * cnt, new_cnt;
  FILE *fp = fopen (fname,"r");

  if ( !fp ){ printf ("file open error\n"); exit (1); }
  T->trsact_num = 0; // set #transactions to 0
  T->item_max = 0; // set max. index of item
  T->eles = 0;
  T->frq = NULL;
  nRow = FILE_read_int (fp);
  if ( (FILE_err&4) != 0){ printf ("file structure error 1\n"); exit (1); }
  nCol = FILE_read_int (fp);
  if ( (FILE_err&4) != 0){ printf ("file structure error 2\n"); exit (1); }
  
  T->eles = nCol;
  T->item_max = 0; //nCol-1;
  T->trsact_num = nRow;
  org_frq = (INT**)alloc_memory(sizeof(INT) * nCol);
	for(i=0; i<nCol; i++)
		org_frq[i] = (INT *)alloc_memory ( sizeof(INT) * (nCol+1) );

	cnt = (size_t*)alloc_memory(sizeof(size_t) * nCol);
	memset(cnt, 0, sizeof(INT)*nCol);
  
	T->buf = (INT *)alloc_memory ( sizeof(INT) * (nRow*nCol) );
  T->trsact = (INT **)alloc_memory ( sizeof(INT*) * nRow );
  j = 0; //row id
	do {
		i = 0; //col id
		T->trsact[j] = &T->buf[j*nCol];
    do {
			item = FILE_read_int (fp);
      if ( (FILE_err&4) == 0){  // got an item from the file before reaching to line-end
	     /* if ( item >= (INT)cnt[i] ){
            new_cnt = cnt[i]*2; if ( item >= (INT)new_cnt ) new_cnt = item +1;
            org_frq[i] = (INT*)realloc_memory((char*)org_frq[i], sizeof(INT), new_cnt, &cnt[i]);
        }
				*/

        if ( item >= T->item_max ) T->item_max = item; //+1;  // uppdate the maximum item
				
				T->buf[j*nCol+i] = item;
				//print_int_arr(T->buf, nCol*nRow, "buf");
				//T->occ[i][item-1][T->occt[i][item-1]] = j;
				org_frq[i++][item]++;
				//i++;
      }
    } while ((FILE_err&3)==0);
		j++;
    //T->trsact_num++;  // increase #transaction
  } while ( (FILE_err&2)==0);

	for(i=0; i<nCol; i++)
		print_int_arr(org_frq[i], T->item_max+1, "org_freq[]");

//	print_int_arr(T->buf, nCol*nRow, "buf");
}

/*	compute the occurrences of all items by occurrence deliver 
	T->occt[i] - mitu korda numbrit i kokku on
	T->occ[i][0...N] = X- numbri i esineb mitmendas (X) transaktsioonis 

*/
void occ_deliv (TRSACT *T, INT col, INT item, INT depth){
  INT t, i, j, *tr, tID, **cur_occt, ***cur_occ;
#ifdef PRINT_DEBUG	
	static int counter;
	printf("counter=%d\n", ++counter);
#endif	
	cur_occt = T->occt_list[depth];
	cur_occ = T->occ_list[depth];
/*
	cur_occt = (INT**) alloc_memory( sizeof(INT) * nCol ); //little overhead, fix later
  T->occt_list[depth] = cur_occt;

	cur_occ = (INT***) alloc_memory(sizeof(INT) * nCol);
	T->occ_list[depth] = cur_occ;
	*/
	INT item_occt = T->occt_list[depth-1][col][item];
	//for ( j=0; j<nCol ; j++ ){
	//	if(T->skip[j]) continue;
	//	cur_occt[j] = (INT*) alloc_memory( sizeof(INT) * (T->item_max+1) );
	//	cur_occ[j] = (INT**) alloc_memory(sizeof(INT) * (T->item_max+1)); //little overhead
	//	for(t=0; t<=T->item_max; t++)
	//		cur_occ[j][t] = (INT*) alloc_memory( sizeof(INT) * item_occt ); //little overhead
	//}

	for ( t=0 ; t<item_occt ; t++ ){
    //tID = T->occ[col][item][t];
		tID = T->occ_list[depth-1][col][item][t];
		for ( j=0; j<nCol ; j++ ){
			if(T->skip[j]) continue;
			INT loc_item = T->buf[tID*nCol+j];
			cur_occ[j][loc_item][cur_occt[j][loc_item]++] = tID;
			//T->occ[j][loc_item][cur_occt[j][loc_item]++] = tID;
#ifdef PRINT_DEBUG				
			printf("loc_col=%d, loc_item=%d, tID=%d, item=%d, col=%d\n", j, loc_item, tID, item, col);
#endif		
		}
	}

	INT ** deb = T->occt_list[depth];
#ifdef PRINT_DEBUG		
	for(i=0;i<nCol;i++)
	{
		if(T->skip[i])continue;
		print_int_arr(deb[i], T->item_max+1, "cur_occt");
	}
#endif	
}

/* main recursive call */
void monsa (TRSACT *T, INT col, INT item, INT depth){
  INT i,j;
	
  //output_itemset (T, T->occt[item]);  // output an itemset
  if( col != -1)
	{
		T->skip[col] = 1;
		occ_deliv (T, col, item, depth);  // compute each item frequency
		//backward comparison
		for(i=0;i<nCol;i++)
		{
			if(T->skip[i])continue;
			for(j=0;j<T->item_max+1;j++)
			{
				if( T->occt_list[depth][i][j] == T->occt_list[depth-1][i][j] )
					T->occt_list[depth-1][i][j] = 0;
			}
		}
	}
	INT max=0, max_item, max_col;
	while(true)
	{
		if( depth == nCol-1 ) break;
		max = 0;
		for(i=0;i<nCol;i++)
		{
			if(T->skip[i])continue;
			for(j=0;j<T->item_max+1;j++)
			{
				if( T->occt_list[depth][i][j] > max)
				{
					max = T->occt_list[depth][i][j];
					max_item = j;
					max_col = i;
				}
			}
		}
		if(max<T->sigma) break;
		monsa(T, max_col, max_item, depth+1);
		T->occt_list[depth][max_col][max_item] = 0;	
#ifdef PRINT_DEBUG		
		for(i=0;i<nCol;i++)
		{
			if(T->skip[i])continue;
			print_int_arr(T->occt_list[depth][i], T->item_max+1, "cur_occt later");
		}
#endif
		//printf("kuradi yks=%d\n", T->occ[0][1][1]);
	}
	T->skip[col] = 0;
	for(i=0;i<nCol;i++)
	{
		memset(T->occt_list[depth][i], 0, sizeof(INT)*(T->item_max+1));
	}
	
  //qsort (&T->jump[jt], T->jump_siz-jt, sizeof(INT), qsort_cmp__idx);  // sort the items 
//  print_int_arr(&T->jump[jt],  T->jump_siz-jt, "T->jump");
//  qsort (&T->jump[jt], T->jump_siz-jt, sizeof(INT), qsort_cmp__idx);  // sort the items 
/*  print_int_arr(&T->jump[jt],  T->jump_siz-jt, "T->jump sorted");
  while ( T->jump_siz > jt ){
    i = T->jump[--T->jump_siz];
    if ( T->occt[i] >= T->sigma ){
      T->itemset[T->itemset_siz++] = i;
//      if ( flag == 0 ){ flag = 1; output_itemset (T, T->occt[i]); }
//      else {
        LCM (T, i);
//      }
      T->itemset_siz--;
    }
    T->occt[i] = 0;
  }*/
}

int main(int argc, char* argv[])
{
	TRSACT T;
	INT i, j;
	size_t cnt;

	if ( argc < 3 ){
		printf ("LCM: enumerate all frequent itemsets\n lcm filename support outputfile\n");
		exit(1);
	}
	T.sigma = atoi(argv[2]);
	TRSACT_file_load (&T, argv[1]);
	//T->occ[i][item-1][T->occt[i][item-1]] = j;
	T.occ = (INT ***) alloc_memory( sizeof(INT)*(nCol+1));
	T.occt = (INT**) alloc_memory( sizeof(INT)*(nCol+1));
	T.occt_list = (INT***) alloc_memory( sizeof(INT)*(nCol+1));

	for(i=0;i<nCol;i++)
	{
		T.occ[i] = (INT**) alloc_memory( sizeof(INT)*(T.item_max+1) );
		T.occt[i] = (INT*) alloc_memory ( sizeof(INT) * (T.item_max+1));
		
		for(j=0;j<=T.item_max;j++)
		{
			T.occ[i][j] = (INT*) alloc_memory(sizeof(INT)*org_frq[i][j]); 
		}
	}

	INT item;
	for(j=0;j<nRow;j++)
	{
		for(i=0; i<nCol; i++)
		{
			item = T.buf[j*nCol+i];
			T.occ[i][item][T.occt[i][item]++] = j;
		}
	}
	
	//meil pole seda enam vaja, T.occt on sama asi
	for(i=0; i<nCol; i++)
		free (org_frq[i]);
	free(org_frq);

	for(i=0;i<nCol;i++)
		print_int_arr(T.occt[i], T.item_max+1, "occt[i]");
	//for(j=0; j<=T.item_max; j++)
	//	for(i=0;i<nCol;i++)
	//		print_int_arr(T.occ[i][j], T.occt[i][j], "occ[i][j]");

	T.occ_list = (INT****) alloc_memory( sizeof(INT)*nCol);
	T.occ_list[0] = T.occ;
	T.skip = (INT*) alloc_memory( sizeof(INT)*nCol);
	T.occt_list[0] = T.occt;

	for(j=1; j<nCol; j++)
	{
		T.occt_list[j] = (INT**) alloc_memory( sizeof(INT) * nCol ); //little overhead, fix later
		T.occ_list[j] = (INT***) alloc_memory(sizeof(INT) * nCol);
	}

	for(i=1;i<nCol;i++)
	{
		for ( j=0; j<nCol ; j++ ){
			T.occt_list[i][j] = (INT*) alloc_memory( sizeof(INT) * (T.item_max+1) );
			T.occ_list[i][j] = (INT**) alloc_memory(sizeof(INT) * (T.item_max+1)); //little overhead
			for(int t=0; t<=T.item_max; t++)
				T.occ_list[i][j][t] = (INT*) alloc_memory( sizeof(INT) * nRow ); //little overhead
		}
	}

	monsa(&T, -1, -1, 0);
	

	return 0;
}

