#include<stdio.h>   //用p数组保存所有的节点，p+*tree指针指向根节点，*tree==0时为空树，len保存节点的个数+1,spare数组保存释放的节点位置
#include<stdlib.h>
#define INSORT 1
#define DELETE 2
#define SEARCH 3
#define MERGE 4
#define SPLIT 5
#define DISPLAY 6

typedef struct node{
	int left_child;
	int right_child;
	int size;
	int name;
}node;
int len=1,spare_top=0;
int spare[100000];
node p[10000];

int SearchSBT(int* tree, int key,node *p,int* pre);
int L_rotate(node *p,int t);
int R_rotate(node *p,int t);
int maintain(node *p,int t);
void SBT_insort(node *p,int *tree,int t);
void SBT_delete(node *p,int *tree,int t); 
void SBT_split(node *p,int t,int *tree,int *new_tree);
int SBT_merge(node *p,int *tree1,int* tree2);
void SBT_display(int *tree,node *p,int layer);

int insort(node *p,int t,int *tree,int loc);
int delete_SBT(node* p,int t,int loc,int *tree,int *pre);
void adjust(node* p,int *q,int len,int len2);
int split(node *p,int t,int loc,int *tree,int *new_tree); 

int main(){
	int key;
	int n,m,i,j,name,location;
	int father=0;
	int tree1,tree2;
	p[0].left_child=0;p[0].name=0;p[0].right_child=0;p[0].size=0;
	printf("please input the number of options\n");
	scanf("%d",&m);
	int** forest = calloc(20,sizeof(int*));
	for(i=0;i<20;i++){
		forest[i] = calloc(1,sizeof(int));
		*(forest[i]) = 0;
	}
	printf("please input 1(insort),2(delete),3(search),4(merge),5(split),6(display) to operate tree\n");
	for(i=0;i<m;i++){
			scanf("%d",&key);
			switch(key){
				case INSORT: scanf("%d%d",&tree1,&name);
				             SBT_insort(p,forest[tree1],name);
				              break;
				case DELETE:  scanf("%d%d",&tree1,&name);
				              SBT_delete(p,forest[tree1],name);
				              break;
				case SEARCH:  scanf("%d%d",&tree1,&name);
				              location = SearchSBT(forest[tree1],name,p,&father);
				              printf("YES,location = %d,its pre_node is %d\n",location,father);
				case MERGE:   scanf("%d%d",&tree1,&tree2);
				              SBT_merge(p,forest[tree1],forest[tree2]);
				              *forest[tree2] = 0;
				              break;
			    case SPLIT:   scanf("%d%d%d",&tree1,&tree2,&name);
			                  SBT_split(p,name,forest[tree1],forest[tree2]);
			                  break;
			    case DISPLAY:  scanf("%d",&tree1);
				              SBT_display(forest[tree1],p,1);
				              break;
			}
			if(spare_top*2>len) {
				adjust(p,spare,len,spare_top);
				len-=spare_top;
				spare_top = 0;
			}
	}
	getchar();
	return 0; 
} 

int SearchSBT(int* tree, int key,node *p,int* pre){ //对树进行查找key值，若找到返回在数组p中的位置，否则返回0,pre指向其父节点
    int tre = *tree;
	if(tre==0) return 0;
	else if(p[tre].name<key){	*pre = *tree; return SearchSBT(&(p[tre].right_child),key,p,pre); }
	else if(p[tre].name>key){	*pre = *tree; return SearchSBT(&(p[tre].left_child),key,p,pre); }
	else return tre;
}

int L_rotate(node *p,int t){  //对树的t节点进行左旋，返回左旋后新树的根节点
	int rc = p[t].right_child;
	p[t].right_child = p[rc].left_child;
	p[rc].left_child = t;
	p[rc].size = p[t].size;
	p[t].size = p[p[t].left_child].size+p[p[t].right_child].size+1;
	return rc;
}

int R_rotate(node *p,int t){
	int rc = p[t].left_child;
	p[t].left_child = p[rc].right_child;
	p[rc].right_child = t;
	p[rc].size = p[t].size;
	p[t].size = p[p[t].left_child].size+p[p[t].right_child].size+1;
	return rc;
}

int maintain(node *p,int t){  //对树的t子树进行平衡处理，返回新树的根节点
    int root=t;
	if(p[p[p[t].left_child].left_child].size > p[p[t].right_child].size){
		root = R_rotate(p,t);
		p[root].right_child = maintain(p,t);
		root = maintain(p,root);
	}
	else if(p[p[p[t].left_child].right_child].size > p[p[t].right_child].size){
		p[t].left_child = L_rotate(p,p[t].left_child);
		root = R_rotate(p,t);
		p[root].left_child = maintain(p,p[root].left_child);
		p[root].right_child = maintain(p,p[root].right_child);
		root = maintain(p,root);
	}
	else if(p[p[p[t].right_child].right_child].size > p[p[t].left_child].size){
		root = L_rotate(p,t);
		p[root].left_child = maintain(p,t);
		root = maintain(p,root);
	}
	else if(p[p[p[t].right_child].left_child].size > p[p[t].left_child].size){
		p[t].right_child = R_rotate(p,p[t].right_child);
		root = L_rotate(p,t);
		p[root].right_child = maintain(p,p[root].right_child);
		p[root].left_child = maintain(p,p[root].left_child);
		root = maintain(p,root);
	}
	return root;
}

int insort(node *p,int t,int *tree,int loc){  //插入节点t（其保存在p[loc]),成功则返回1，失败则返回0
    int tre = *tree,flag;
    if(tre == 0) {
		(*tree)++;
		p[1].name = t; p[1].size = 1; p[1].left_child=0; p[1].right_child = 0;
	}
	else if(t < p[tre].name){
		if(p[tre].left_child == 0){
			p[tre].left_child = loc; p[tre].size++;
		}
		else {
			p[tre].size++;
			if(!insort(p,t,&(p[tre].left_child),loc)) return 0;
			(*tree) = maintain(p,tre);
		}
	}
	else if(t > p[tre].name){
		if(p[tre].right_child == 0){
			p[tre].right_child = loc; p[tre].size++;
		}
		else {
			p[tre].size++;
			if(!insort(p,t,&(p[tre].right_child),loc)) return 0;
			(*tree) = maintain(p,tre);
		}
	}
	return 1;
}

int delete_SBT(node* p,int t,int loc,int *tree,int *pre){ //成功删除t（其保存在p[loc])返回1，否则返回0
        int tre = *tree;
		if(tre == loc){
			if(p[tre].left_child == 0&&p[tre].right_child == 0) {
				if(*pre == tre){*pre = 0;	*tree = 0;	}
				else{ *tree = 0;}
				return 1;
			}
			else if(p[tre].left_child != 0&&p[tre].right_child == 0){
				if(*pre == tre){
					*tree = p[tre].left_child; *pre = 0;
				}
				else{  *tree = p[tre].left_child;		}
				return 1;
			}
			else if(p[tre].right_child != 0&&p[tre].left_child == 0){
				if(*pre == tre){
					*tree = p[tre].right_child; *pre = 0;
				}
				else{ *tree = p[tre].right_child; }
				return 1;
			}
			else{
				int prenode = p[tre].left_child;
				int father = tre;
				while(p[prenode].right_child != 0){ father = prenode; prenode = p[prenode].right_child;}
				if(*pre==tre){	*tree = prenode;	}
				else if(p[*pre].left_child==tre) {		p[*pre].left_child = prenode;	}
				else {		p[*pre].right_child = prenode;	}
				p[prenode].right_child = p[tre].right_child;
				p[prenode].size = p[tre].size-1;
				if(p[tre].left_child == prenode) ;
				else{
					p[father].right_child = p[prenode].left_child;
					p[prenode].left_child = p[tre].left_child;
				}
				*tree = maintain(p,prenode); return 1;
			}
		}
		else if(t<p[tre].name){
			p[tre].size--;
			if(delete_SBT(p,t,loc,&(p[tre].left_child),pre)) {*tree = maintain(p,tre); return 1;}
		}
		else{
			p[tre].size--;
			if(delete_SBT(p,t,loc,&(p[tre].right_child),pre)) {*tree = maintain(p,tre); return 1;}
		}
		return 0;
}

void SBT_display(int *tree,node* p,int layer){   //中序遍历 
    int i;
	if(p[*tree].left_child!=0)  {
		SBT_display(&p[*tree].left_child,p,layer+1);
	}
	for(i=0;i<layer;i++) printf("@");
	printf("%d %d\n",p[*tree].name,p[*tree].size);
	if(p[*tree].right_child!=0){
		SBT_display(&p[*tree].right_child,p,layer+1);
	}
}

int cmp(const void *a,const void *b){
	return (*(int*)a-*(int*)b);
}
void adjust(node* p,int *q,int len_p,int len_q){
	int i,j,k;
	len_p--;
	qsort(q,len_q,sizeof(q[0]),cmp);
	for(i=0,j=0,k=0;i<len_p;i++){
		if(i==q[k]) k++;
		else{
			p[j++] = p[i];
		}
	}
}

int split(node *p,int t,int loc,int *tree,int *new_tree){
	int tre = *tree;
	int size = p[p[loc].right_child].size+1;
	if(p[tre].name<t){
		p[tre].size-=size;
		if(split(p,t,loc,&(p[tre].right_child),new_tree)){
			*tree = maintain(p,tre);
			return 1;
		}
	}
	else if(p[tre].name>t){
		p[tre].size-=size;
		if(split(p,t,loc,&(p[tre].left_child),new_tree)){
			*tree = maintain(p,tre);
			return 1;
		}
	}
	else{
		*tree = p[loc].left_child;
		p[tre].size-=p[p[tre].left_child].size;
		p[tre].left_child = 0;
		*new_tree = maintain(p,tre);
		return 1;
	}
	return 0;
}

int SBT_merge(node *p,int *tree1,int* tree2){
	int tre2=*tree2;
	if(p[tre2].right_child!=0){
		if(!SBT_merge(p,tree1,&(p[tre2].right_child))) return 0;
	}
	if(p[tre2].left_child!=0){
		if(!SBT_merge(p,tree1,&(p[tre2].left_child))) return 0;
	}
	p[tre2].left_child = 0;
	p[tre2].right_child = 0;
	p[tre2].size = 1;
	if(!insort(p,p[tre2].name,tree1,tre2)) return 0;
	return 1;
}

void SBT_insort(node *p,int *tree,int t){
	int father=0,location;
	if(SearchSBT(tree,t,p,&father)==0){
	    if(spare_top>0){	location = spare[--spare_top];	}
	    else {	location = len; len++;	}
	   	p[location].left_child=0; p[location].right_child = 0; p[location].name = t; p[location].size=1;
	  	insort(p,t,tree,location);
	}
}

void SBT_delete(node *p,int *tree,int t){
	int father;
	int location = SearchSBT(tree,t,p,&father);
	spare[spare_top++] = location;
	if(location!=0) delete_SBT(p,t,location,tree,&father);
}

void SBT_split(node *p,int t,int *tree,int *new_tree){
	int father;
	*new_tree = 0;
	int location = SearchSBT(tree,t,p,&father);
	split(p,t,location,tree,new_tree);
}
