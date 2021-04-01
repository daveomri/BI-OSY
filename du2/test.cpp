#ifndef __PROGTEST__
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cmath>
using namespace std;
#endif /* __PROGTEST__ */

//Global variables
int memorySize = 0;
void * memStart = NULL;
void * memEnd = NULL;
int buddySize = 0;
int allocBlock = 0;

const int minBlockSize = 5;
const int maxBlockSize = 30;
const int headerSize = 18;


struct BuddyArray{
  uint64_t* free    = NULL;
  uint64_t* picked  = NULL;
};

BuddyArray buddyArray[maxBlockSize];

//----------------------------------------------------------

int getIndex(int i){
  if (i < minBlockSize)
    return -1;
  return (i-minBlockSize);
}


int findU(int memSize){
  //minimal size of memory block
  int tmpInt = 32;
  //result
  int result = 0;
  
  for (int i = 5; i < 32; i++){
    if (memSize < tmpInt){
      result = i;
      break;
    }

    tmpInt<<=1;
  }
  //return U
  return result-1;
}

int findGEU(int memSize){
  //minimal size of memory block
  int tmpInt = 32;
  //result
  int result = 0;
  
  for (int i = 5; i < 32; i++){
    if (memSize <= tmpInt){
      result = i;
      break;
    }

    tmpInt<<=1;
  }
  //return U
  return result;
}


//----------------------------------------------------------
void   HeapInit    ( void * memPool, int memSize )
{
  //Store important info
  memStart = memPool;
  memEnd = (void*)(((uint8_t*)(memPool)+memSize));
  memorySize = memSize;

  //Alocate first block
  int U = findU(memSize);
  int mvIndex = getIndex(U);

  //make table
  buddySize = U;
  uint64_t * mvBlock = (uint64_t*)(memPool);
  //alocate table with pointers
  if (mvBlock<0)
    return;

  
  //set first block
  buddyArray[mvIndex].free = mvBlock;

  //current buddy
  //uint8_t buddy = 0;
  //create new header
  buddyArray[mvIndex].free = mvBlock;
  buddyArray[mvIndex].picked = NULL;

  mvBlock[0] = (uint64_t)(buddyArray+mvIndex);
  mvBlock[1] = (uint64_t)(NULL);
  uint8_t * eightMove = (uint8_t*)(mvBlock);
  eightMove[16] = '\0';//free or not
  eightMove[17] = (uint8_t)mvIndex;//kin
  
  //append the rest
  int realSize = pow(2, U);
  int restSize = memorySize-realSize;
  while ((restSize) > 31){
    
    U = findU(restSize);
    mvIndex = getIndex(U);   
    
    //buddy+=1;
    eightMove = (uint8_t*)(memPool);    

    //set new header
    if (mvIndex >= 0){
      uint64_t * tmpBlock = (uint64_t*)(eightMove+realSize);
      tmpBlock[0] = (uint64_t)( buddyArray+mvIndex );//prev change
      tmpBlock[1] = (uint64_t)( (buddyArray[mvIndex].free ) );//next
      (eightMove+realSize+16)[0] = '\0';
      (eightMove+realSize+17)[0] = (uint8_t)mvIndex;

      buddyArray[mvIndex].free = (tmpBlock);

      realSize+=pow(2, U);
      restSize = memorySize - realSize;
    }
  }
}




//---------------------------------------------------------------



void moveDown( int to ){
  int from = -1;
  //find closest block
  for ( int i = to; i < buddySize; i++ ){
    if(buddyArray[i].free!=NULL){
      from = i;
      break;
    }
  }

  if (from == -1)
    return;

  //reconnect
  uint64_t*tmp = buddyArray[from].free;
  
  buddyArray[from].free = reinterpret_cast<uint64_t*>(tmp[1]);
  if (buddyArray[from].free)
    buddyArray[from].free[0] = (uint64_t)(buddyArray+from);
  
  uint64_t*tmp_1 = NULL;

  //move down to to
  for (int i = from-1; i >= to; i--){
    //copy old
    tmp_1 = reinterpret_cast<uint64_t*>(((uint64_t)tmp)+(uint64_t)(pow(2, i+5)));
    
    tmp_1[0] = (uint64_t)(buddyArray+i);
    
    tmp_1[1] = (uint64_t)(buddyArray[i].free);

    ((uint8_t*)tmp_1)[16] = ((uint8_t*)(tmp))[16];
    ((uint8_t*)tmp_1)[17] = (uint8_t)i;

    //reconnect
    if (buddyArray[i].free)
      buddyArray[i].free[0] = (uint64_t)(tmp_1);
    buddyArray[i].free = tmp_1;
  }

  //connect the last box
  tmp[0] = (uint64_t)(buddyArray+to);
  tmp[1] = (uint64_t)(buddyArray[to].free);
  ((uint8_t*)tmp)[17] = (uint8_t)to;

  //reconnect
  if (buddyArray[to].free)
    buddyArray[to].free[0] = (uint64_t)(tmp);
  buddyArray[to].free = tmp;
}



//---------------------------------------------------------------

void * HeapAlloc   ( int size )
{
  if (size <= 0)
    return NULL;

  //find best index
  int neededSize = findU(size+headerSize)+1;//int neededSize = findGEU(size+headerSize);
  //printf("0 far\n");
  if (neededSize < minBlockSize)
    return NULL;
  // printf("1 far\n");

  if(buddyArray[getIndex(neededSize)].free==NULL){
    moveDown(getIndex(neededSize));
  }

  uint64_t* result = (buddyArray[getIndex(neededSize)].free);

  //no block to return
  if (result == NULL)
    return NULL;
  //printf("2 far\n");

  //prepare block
  buddyArray[getIndex(neededSize)].free = reinterpret_cast<uint64_t*>( (buddyArray[getIndex(neededSize)].free)[1] );
  if (buddyArray[getIndex(neededSize)].free)
    buddyArray[getIndex(neededSize)].free[0] = (uint64_t)(buddyArray+getIndex(neededSize));

  if (buddyArray[getIndex(neededSize)].picked)
    buddyArray[getIndex(neededSize)].picked[0] = (uint64_t)(result);

  //insert to picked array
  result[0] = (uint64_t)(buddyArray+getIndex(neededSize));
  result[1] = (uint64_t)(buddyArray[getIndex(neededSize)].picked);
  uint8_t * tmpBlock = (uint8_t*)result;
  tmpBlock[16] = '\1'; //taken

  buddyArray[getIndex(neededSize)].picked = result;

  allocBlock+=1;
  return ((uint8_t*)result)+headerSize;
}



//---------------------------------------------------------------------------

bool testPointer( uint8_t * pt, int free = 0 ){
  //test range
  if ( pt >= (uint8_t*)(memEnd) ||  pt < (uint8_t*)(memStart)+headerSize )
    return false;
  
  //test if it is avalible to free
  if ( free == 0 ){
    if ( (pt-2)[0] != '\1'  )
      return false;
  }
  else{
    if ( (pt-2)[0] != '\0'  )
      return false;
  }

  uint64_t* tmpBlock = (uint64_t*)(pt-headerSize);
  uint64_t* tmpBlockSec = NULL;
  if ( (tmpBlock[0] >= (uint64_t)(memStart) && tmpBlock[0] < (uint64_t)(memEnd)) &&
       ((tmpBlock[1] >= (uint64_t)(memStart) && tmpBlock[1] < (uint64_t)(memEnd)) ||
       tmpBlock[1] == (uint64_t)NULL) ){
    tmpBlockSec = reinterpret_cast<uint64_t*>(tmpBlock[0]);
    //test left neighbor
    if ( tmpBlockSec+1 >= (uint64_t*)memEnd || tmpBlockSec[1] != (uint64_t)tmpBlock)
      return false;

    //test right neighbor
    if (tmpBlock[1]!=(uint64_t)NULL){
      tmpBlockSec = reinterpret_cast<uint64_t*>(tmpBlock[1]);

      if ( tmpBlockSec[0] != (uint64_t)tmpBlock )
        return false;
    }

    return true;
  }
  else if ( (tmpBlock[0] >= (uint64_t)(buddyArray) && tmpBlock[0] < (uint64_t)(buddyArray+buddySize) ) &&
            ((tmpBlock[1] >= (uint64_t)(memStart) && tmpBlock[1] < (uint64_t)(memEnd)) ||
            (tmpBlock[1] == (uint64_t)NULL) ) ){
    
    BuddyArray * tmpArr = reinterpret_cast<BuddyArray*>(tmpBlock[0]);

    if ( tmpArr->picked != tmpBlock)
      return false;

    if (tmpBlock[1]!=(uint64_t)NULL){
      tmpBlockSec = reinterpret_cast<uint64_t*>(tmpBlock[1]);

      if ( tmpBlockSec[0] != (uint64_t)tmpBlock )
        return false;
    }

    return true;
  }
  return false;
}




/*
uint64_t * getNearestMerge( int power, uint8_t* toMerge ){
  uint8_t * curBlock = (uint8_t*)toMerge;
  uint8_t * movBlock = (uint8_t*)memStart;
  uint64_t* result;

  int res =   (((uint64_t)toMerge)-(uint64_t)(movBlock));
  res     /=  pow(2, power);

  if (res%2==1){//odd | pick left
    result = (uint64_t*)(curBlock-((int)pow(2, power)) );

    if ( result >= (uint64_t*)memStart && ((uint8_t*)result)[17] == power-5 ){
      
      if ( testPointer( (uint8_t*)result + headerSize, 1 ) )
        return result;
        
      return NULL;
    }
     
    return NULL;
  }
  else{//even | pick right
    result = (uint64_t*)(curBlock+((int)pow(2, power)) );

    if ( ((uint8_t*)result+17) < (uint8_t*)memEnd && ((uint8_t*)result)[17] == power-5  ){
      
      if ( testPointer( (uint8_t*)result + headerSize, 1 ) )
        return result;

      return NULL;
    }
     
    return NULL;
  }
}
*/
uint64_t * getNearestMerge( int power, uint8_t* toMerge ){
  uint8_t * curBlock = (uint8_t*)toMerge;
  uint8_t * movBlock = (uint8_t*)memStart;

  int res =   (((uint64_t)toMerge)-(uint64_t)(movBlock));
  res     /=  pow(2, power);

  if (res%2==1){//odd | pick left
    return (uint64_t*)(curBlock-((int)pow(2, power)) );
  }
  else{//even | pick rigt
    return (uint64_t*)(curBlock+((int)pow(2, power)) );
  }
}






void disconnectBlock( uint64_t* block, int pick ){
  BuddyArray * arrayBlock = NULL;
  if ( block[0]>=(uint64_t)buddyArray && block[0] < (uint64_t)(buddyArray+buddySize) ){
    arrayBlock = reinterpret_cast<BuddyArray*>(block[0]);

    if (block[1]!=(uint64_t)NULL){
      (reinterpret_cast<uint64_t*>(block[1]))[0] = (uint64_t)(arrayBlock);
    }
    if (pick == 1)
      arrayBlock->picked  = reinterpret_cast<uint64_t*>(block[1]);
    else
      arrayBlock->free    = reinterpret_cast<uint64_t*>(block[1]);
  }
  else{
    (reinterpret_cast<uint64_t*>(block[0]))[1] = block[1];
    if (block[1]!=(uint64_t)NULL)
      (reinterpret_cast<uint64_t*>(block[1]))[0] = block[0];
  }

  block[0] = (uint64_t)(NULL);
  block[1] = (uint64_t)(NULL);

}

bool   HeapFree    ( void * blk )
{
  //test pointer
  if (blk == NULL)
    return false;
  
  if ( !testPointer( ((uint8_t*)blk) ) )
    return false;
  
  uint64_t*tmpBlock = (uint64_t*)( (uint8_t*)(blk)-headerSize );
  
  
  disconnectBlock(tmpBlock, 1);


  //set status
  ((uint8_t*)(tmpBlock))[16] = '\0';
 
  
  BuddyArray * arrayBlock = NULL;
  //while ( !(tmpBlock[0]>=(uint64_t)buddyArray && tmpBlock[0]< (uint64_t)(buddyArray+buddySize)) )
  //  tmpBlock = reinterpret_cast<uint64_t*>(tmpBlock[0]);
  arrayBlock = buddyArray+((uint8_t*)(tmpBlock))[17];
  
  int power = (uint64_t)(arrayBlock-buddyArray);

  uint64_t * nearestBlock;

  uint64_t * movingBlock;

  int foundBlock = 0;

  uint64_t* prevBlock = (uint64_t*)( (uint8_t*)(blk)-headerSize );
  //disconnect on levels
  
  int realPower = power;
  for (int i = power; i < buddySize; i++){
    
    nearestBlock = getNearestMerge(i+5, (uint8_t*)(prevBlock) );

    arrayBlock = (buddyArray+i);

    movingBlock = arrayBlock->free;

    foundBlock = 0;

    while (movingBlock!=NULL){
      if (movingBlock == nearestBlock){
        foundBlock = 1;
        break;
      }
      movingBlock = reinterpret_cast<uint64_t*>(movingBlock[1]);
    }

    if ( foundBlock == 1 ){//found merge
      realPower+=1;
      disconnectBlock(nearestBlock, 0);
      if (nearestBlock < prevBlock){
        prevBlock = nearestBlock;
      }
      ((uint8_t*)prevBlock)[17] = (uint8_t)realPower;
    }
    else{//no more merge
      break;
    }
  }

  //insert to free
  prevBlock[1] = (uint64_t)(buddyArray[realPower].free);
  prevBlock[0] = (uint64_t)(buddyArray+realPower);

  if (buddyArray[realPower].free)
    buddyArray[realPower].free[0] = (uint64_t)prevBlock;
  buddyArray[realPower].free = prevBlock;

  allocBlock-=1; 
  return true;
}

//--------------------------------------------------------------------------

void   HeapDone    ( int  * pendingBlk )
{
  *pendingBlk = allocBlock;
  allocBlock = 0;
  //clean mess
  for (int i = 0; i < buddySize; i++){
    buddyArray[i].free = NULL;
    buddyArray[i].picked = NULL;
  }
}

//--------------------------------------------------------------------------


#ifndef __PROGTEST__
int main ( void )
{
  uint8_t       * p0, *p1, *p2, *p3, *p4;
  int             pendingBlk;
  static uint8_t  memPool[3 * 1048576];
  

  HeapInit ( memPool, 2097152 );
  assert ( ( p0 = (uint8_t*) HeapAlloc ( 512000 ) ) != NULL );
  memset ( p0, 0, 512000 );
  assert ( ( p1 = (uint8_t*) HeapAlloc ( 511000 ) ) != NULL );
  memset ( p1, 0, 511000 );
  assert ( ( p2 = (uint8_t*) HeapAlloc ( 26000 ) ) != NULL );
  memset ( p2, 0, 26000 );
  HeapDone ( &pendingBlk );
  assert ( pendingBlk == 3 );


  HeapInit ( memPool, 2097152 );
  assert ( ( p0 = (uint8_t*) HeapAlloc ( 1000000 ) ) != NULL );
  memset ( p0, 0, 1000000 );
  assert ( ( p1 = (uint8_t*) HeapAlloc ( 250000 ) ) != NULL );
  memset ( p1, 0, 250000 );
  assert ( ( p2 = (uint8_t*) HeapAlloc ( 250000 ) ) != NULL );
  memset ( p2, 0, 250000 );
  assert ( ( p3 = (uint8_t*) HeapAlloc ( 250000 ) ) != NULL );
  memset ( p3, 0, 250000 );
  assert ( ( p4 = (uint8_t*) HeapAlloc ( 50000 ) ) != NULL );
  memset ( p4, 0, 50000 );
  assert ( HeapFree ( p2 ) );
  assert ( HeapFree ( p4 ) );
  assert ( HeapFree ( p3 ) );
  assert ( HeapFree ( p1 ) );
  assert ( ( p1 = (uint8_t*) HeapAlloc ( 500000 ) ) != NULL );
  memset ( p1, 0, 500000 );
  assert ( HeapFree ( p0 ) );
  assert ( HeapFree ( p1 ) );
  HeapDone ( &pendingBlk );
  assert ( pendingBlk == 0 );


  HeapInit ( memPool, 2359296 );
  assert ( ( p0 = (uint8_t*) HeapAlloc ( 1000000 ) ) != NULL );
  memset ( p0, 0, 1000000 );
  assert ( ( p1 = (uint8_t*) HeapAlloc ( 500000 ) ) != NULL );
  memset ( p1, 0, 500000 );
  assert ( ( p2 = (uint8_t*) HeapAlloc ( 500000 ) ) != NULL );
  memset ( p2, 0, 500000 );
  assert ( ( p3 = (uint8_t*) HeapAlloc ( 500000 ) ) == NULL );
  assert ( HeapFree ( p2 ) );
  assert ( ( p2 = (uint8_t*) HeapAlloc ( 300000 ) ) != NULL );
  memset ( p2, 0, 300000 );
  assert ( HeapFree ( p0 ) );
  assert ( HeapFree ( p1 ) );
  HeapDone ( &pendingBlk );
  assert ( pendingBlk == 1 );


  HeapInit ( memPool, 2359296 );
  assert ( ( p0 = (uint8_t*) HeapAlloc ( 1000000 ) ) != NULL );
  memset ( p0, 0, 1000000 );
  assert ( ! HeapFree ( p0 + 1000 ) );
  HeapDone ( &pendingBlk );
  assert ( pendingBlk == 1 );
  

  return 0;
}
#endif /* __PROGTEST__ */