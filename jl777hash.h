//
//  jl777hash.h
//  Created by jl777
//  MIT License
//

#ifndef gateway_jl777hash_h
#define gateway_jl777hash_h

#define HASHTABLES_STARTSIZE 100
#define HASHSEARCH_ERROR ((uint64_t)-1)
#define HASHTABLE_FULL .666
struct hashtable
{
    char *name;
    void **hashtable;
    uint64_t hashsize,numsearches,numiterations,numitems;
    unsigned long keyoffset,keysize,modifiedoffset,structsize;
};

struct hashpacket
{
    int32_t *createdflagp,doneflag,funcid;
    char *key;
    struct hashtable **hp_ptr;
    union { void *result; uint64_t hashval; };
};

extern int32_t Historical_done;

uint64_t calc_decimalhash(char *key)
{
    char c;
    uint64_t i,a,b,hashval = 0;
    a = 63689; b = 378551;
    for (i=0; key[i]!=0; i++,a*=b)
    {
        c = key[i];
        if ( c >= '0' && c <= '9' )
            hashval = hashval*a + (c - '0');
    }
    return(hashval);
}

struct hashtable *hashtable_create(char *name,int64_t hashsize,long structsize,long keyoffset,long keysize,long modifiedoffset)
{
    struct hashtable *hp;
    hp = calloc(1,sizeof(*hp));
    hp->name = clonestr(name);
    hp->hashsize = hashsize;
    hp->structsize = structsize;
    hp->keyoffset = keyoffset;
    hp->keysize = keysize;
    hp->modifiedoffset = modifiedoffset;
    hp->hashtable = calloc(hp->hashsize,sizeof(*hp->hashtable));
    return(hp);
}

int64_t _hashtable_clear_modified(struct hashtable *hp,long offset) // no MT support (yet)
{
    void *ptr;
    uint64_t i,nonz = 0;
    for (i=0; i<hp->hashsize; i++)
    {
        ptr = hp->hashtable[i];
        if ( ptr != 0 && *(int64_t *)((long)ptr + offset) != 0 )
            *(int64_t *)((long)ptr + offset) = 0, nonz++;
    }
    return(nonz);
}
#define hashtable_clear_modified(hp)_hashtable_clear_modified(hp,(hp)->modifiedoffset)

void **hashtable_gather_modified(int64_t *changedp,struct hashtable *hp,int32_t forceflag) // no MT support (yet)
{
    uint64_t i,m,n = 0;
    void *ptr,**list = 0;
    if ( hp == 0 )
        return(0);
    for (i=0; i<hp->hashsize; i++)
    {
        ptr = hp->hashtable[i];
        if ( ptr != 0 && (forceflag != 0 || *(int64_t *)((long)ptr + hp->modifiedoffset) != 0) )
            n++;
    }
    if ( n != 0 )
    {
        list = calloc(n+1,sizeof(*list));
        for (i=m=0; i<hp->hashsize; i++)
        {
            ptr = hp->hashtable[i];
            if ( ptr != 0 && (forceflag != 0 || *(int64_t *)((long)ptr + hp->modifiedoffset) != 0) )
                list[m++] = ptr;
        }
        if ( m != n )
            printf("gather_modified: unexpected m.%ld != n.%ld\n",(long)m,(long)n);
    }
    *changedp = n;
    return(list);
}

uint64_t search_hashtable(struct hashtable *hp,char *key)
{
    void *ptr,**hashtable = hp->hashtable;
    uint64_t i,hashval,ind;
    if ( hp == 0 )
        return(HASHSEARCH_ERROR);
    hashval = calc_decimalhash(key);
    //printf("hashval = %ld\n",(unsigned long)hashval);
    ind = (hashval % hp->hashsize);
    hp->numsearches++;
    if ( (hp->numsearches % 100000) == 0 )
        printf("search_hashtable  ave %.3f numsearches.%ld numiterations.%ld\n",(double)hp->numiterations/hp->numsearches,(long)hp->numsearches,(long)hp->numiterations);
    for (i=0; i<hp->hashsize; i++,ind++)
    {
        hp->numiterations++;
        if ( ind >= hp->hashsize )
            ind = 0;
        ptr = hashtable[ind];
        if ( ptr == 0 )
            return(ind);
        if ( strcmp((void *)((long)ptr + hp->keyoffset),key) == 0 )
            return(ind);
    }
    return(HASHSEARCH_ERROR);
}

struct hashtable *resize_hashtable(struct hashtable *hp,int64_t newsize)
{
    void *ptr;
    uint64_t ind,newind;
    struct hashtable *newhp = calloc(1,sizeof(*newhp));
    *newhp = *hp;
    //printf("about to resize %s %ld, hp.%p\n",hp->name,(long)hp->numitems,hp);
    newhp->hashsize = newsize;
    newhp->numitems = 0;
    newhp->hashtable = calloc(newhp->hashsize,sizeof(*newhp->hashtable));
    for (ind=0; ind<hp->hashsize; ind++)
    {
        ptr = hp->hashtable[ind];
        if ( ptr != 0 )
        {
            newind = search_hashtable(newhp,(char *)((long)ptr + newhp->keyoffset));
            if ( newind != HASHSEARCH_ERROR && newhp->hashtable[newind] == 0 )
            {
                newhp->hashtable[newind] = ptr;
                newhp->numitems++;
                //printf("%ld old.%ld -> new.%ld\n",(long)newhp->numitems,(long)ind,(long)newind);
            } else printf("duplicate entry?? at ind.%ld newind.%ld\n",(long)ind,(long)newind);
        }
    }
    if ( hp->numitems != newhp->numitems )
        printf("RESIZE ERROR??? %ld != %ld\n",(long)hp->numitems,(long)newhp->numitems);
    else
    {
        for (ind=0; ind<hp->hashsize; ind++)
        {
            ptr = hp->hashtable[ind];
            if ( ptr != 0 )
            {
                newind = search_hashtable(newhp,(char *)((long)ptr + newhp->keyoffset));
                if ( newind == HASHSEARCH_ERROR || newhp->hashtable[newind] == 0 )
                    printf("ERROR: cant find %s after resizing\n",(char *)((long)ptr + hp->keyoffset));
            }
        }
    }
    //printf("free hp.%p\n",hp);
    free(hp->hashtable);
    free(hp);
    //printf("finished %s resized to %ld\n",newhp->name,(long)newhp->hashsize);
    return(newhp);
}

void *add_hashtable(int32_t *createdflagp,struct hashtable **hp_ptr,char *key)
{
    void *ptr;
    uint64_t ind;
    struct hashtable *hp = *hp_ptr;
    *createdflagp = 0;
    if ( key == 0 || *key == 0 || hp == 0 || strlen(key) >= hp->keysize )
    {
        printf("%p key.(%s) len.%ld is too big for %s %ld\n",key,key,strlen(key),hp->name,hp->keysize);
        return(0);
    }
    //printf("hp %p %p hashsize.%ld add_hashtable(%s)\n",hp_ptr,hp,(long)hp->hashsize,key);
    if ( hp->hashtable == 0 )
        hp->hashtable = calloc(sizeof(*hp->hashtable),hp->hashsize);
    else if ( hp->numitems > hp->hashsize*HASHTABLE_FULL )
    {
        *hp_ptr = resize_hashtable(hp,hp->hashsize * 3);
        hp = *hp_ptr;
    }
    ind = search_hashtable(hp,key);
    if ( ind == HASHSEARCH_ERROR ) // table full
        return(0);
    ptr = hp->hashtable[ind];
    //printf("ptr %p, ind.%ld\n",ptr,(long)ind);
    if ( ptr == 0 )
    {
        ptr = calloc(1,hp->structsize);
        hp->hashtable[ind] = ptr;
        strcpy((void *)((long)ptr + hp->keyoffset),key);
        //*(int64_t *)((long)ptr + hp->modifiedoffset) = 1;
        hp->numitems++;
        *createdflagp = 1;
        ind = search_hashtable(hp,key);
        if ( ind == (uint64_t)-1 || hp->hashtable[ind] == 0 )
            printf("FATAL ERROR adding (%s) to hashtable.%s\n",key,hp->name);
    }
    return(ptr);
}

void *MTadd_hashtable(int32_t *createdflagp,struct hashtable **hp_ptr,char *key)
{
    void *result;
    extern struct NXThandler_info *Global_mp;
    struct hashpacket *ptr;
    if ( Historical_done != 0 )
        return(add_hashtable(createdflagp,hp_ptr,key));
    else
    {
        //pthread_mutex_lock(&Global_mp->hash_mutex);
        ptr = calloc(1,sizeof(*ptr));
        ptr->createdflagp = createdflagp;
        ptr->hp_ptr = hp_ptr;
        ptr->key = key;
        ptr->funcid = 'A';
        queue_enqueue(&Global_mp->hashtable_queue,ptr);
        //pthread_mutex_unlock(&Global_mp->hash_mutex);
        //printf("hashsize.%ld %p queued2 hp_ptr %p\n",(long)(*hp_ptr)->hashsize,ptr,hp_ptr);
        while ( ptr->doneflag == 0 )
            usleep(1);
        result = ptr->result;
        free(ptr);
        return(result);
    }
}

uint64_t MTsearch_hashtable(struct hashtable **hp_ptr,char *key)
{
    uint64_t hashval;
    struct hashtable *hp = *hp_ptr;
    struct hashpacket *ptr;
    if ( Historical_done != 0 )
        return(search_hashtable(hp,key));
    else
    {
        //pthread_mutex_lock(&Global_mp->hash_mutex);
        ptr = calloc(1,sizeof(*ptr));
        ptr->hp_ptr = hp_ptr;
        ptr->key = key;
        ptr->funcid = 'S';
        queue_enqueue(&Global_mp->hashtable_queue,ptr);
        //pthread_mutex_unlock(&Global_mp->hash_mutex);
        //printf("hashsize.%ld %p queued hp_ptr %p\n",(long)(*hp_ptr)->hashsize,ptr,hp_ptr);
        while ( ptr->doneflag == 0 )
            usleep(1);
        hashval = ptr->hashval;
        free(ptr);
        return(hashval);
    }
}

void *process_hashtablequeues(void *_p) // serialize hashtable functions
{
    struct hashpacket *ptr;
    while ( 1 )//Historical_done == 0 )
    {
        usleep(1 + Historical_done*1000);
        //pthread_mutex_lock(&Global_mp->hash_mutex);
        while ( (ptr= queue_dequeue(&Global_mp->hashtable_queue)) != 0 )
        {
            //printf("numitems.%ld process.%p hp %p\n",(long)(*ptr->hp_ptr)->hashsize,ptr,ptr->hp_ptr);
            if ( ptr->funcid == 'A' )
                ptr->result = add_hashtable(ptr->createdflagp,ptr->hp_ptr,ptr->key);
            else if ( ptr->funcid == 'S' )
                ptr->hashval = search_hashtable(*ptr->hp_ptr,ptr->key);
            else printf("UNEXPECTED MThashtable funcid.(%c) %d\n",ptr->funcid,ptr->funcid);
            ptr->doneflag = 1;
        }
        //pthread_mutex_unlock(&Global_mp->hash_mutex);
    }
    printf("finished processing hashtable MT queues\n");
    exit(0);
    return(0);
}
#endif
