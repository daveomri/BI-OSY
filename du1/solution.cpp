#ifndef __PROGTEST__
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <climits>
#include <cfloat>
#include <cassert>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <vector>
#include <set>
#include <list>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <stack>
#include <deque>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include "progtest_solver.h"
#include "sample_tester.h"
using namespace std;
#endif /* __PROGTEST__ */ 

class CSentinelMessage{
  public:
                                CSentinelMessage(uint32_t);
    CBigInt                *    CPrefCount;
    vector<uint64_t>            CFragments;
    uint32_t                    CId;

    void                        push(uint64_t);
    static uint32_t             getFragmentId(uint64_t);
};
CSentinelMessage::CSentinelMessage(uint32_t CId) : CId(CId) {}
void CSentinelMessage::push(uint64_t data){
  CFragments.push_back(data);
}
uint32_t CSentinelMessage::getFragmentId(uint64_t frag){
  uint32_t output = (frag >> 37);
  return output;
}
//---------------------------------------------------------------------------
class CSharedBox{
  public:
                                CSharedBox(uint32_t, bool );
    uint32_t                    origin;
    uint64_t                    fragments;
    CBigInt                     res;
    bool                        die;

    void                        setRes( CBigInt& );
    void                        setFragments( uint64_t& );
};

CSharedBox::CSharedBox( uint32_t origin, bool die ) : origin(origin), die(die){}

void CSharedBox::setRes( CBigInt & bigInt  ){
  res = bigInt;
}

void CSharedBox::setFragments( uint64_t & vecFrag ){
  fragments = vecFrag;
}
//---------------------------------------------------------------------------
class CSentinelBuffer{
  private:
    deque<CSharedBox*>            CBuffer;
    size_t                        CBuffSize;
    mutex                         CMutex;
    condition_variable            CBuffFull;
    condition_variable            CBuffEmpt;
  public:
    

                            CSentinelBuffer(size_t);
                            ~CSentinelBuffer();
    void                    push_back(CSharedBox*);
    CSharedBox *            pop_front();
    bool                    empty();
    int                     size();
};

CSentinelBuffer::~CSentinelBuffer(){
}

CSentinelBuffer::CSentinelBuffer(size_t size = 100) : CBuffSize(size){}

void CSentinelBuffer::push_back(CSharedBox * msg){
  unique_lock<mutex> unqLck(CMutex);
  //wait till there is free space
  CBuffFull.wait(unqLck, [this] () { return CBuffer.size() < CBuffSize;} );
  //push item to back
  CBuffer.push_back(msg);
  //one place has been filled
  CBuffEmpt.notify_one();
}

CSharedBox * CSentinelBuffer::pop_front(){
  unique_lock<mutex> unqLck(CMutex);
  CSharedBox * outMsg = NULL;
  //wait for item
  CBuffEmpt.wait(unqLck, [this] () {return !CBuffer.empty();});
  //remove item from front
  outMsg = CBuffer.front();
  CBuffer.pop_front();
 
  //one place has been freed
  CBuffFull.notify_one(); 
  return outMsg;
}

bool CSentinelBuffer::empty(){
  return CBuffer.empty();
}

int CSentinelBuffer::size(){
  return CBuffer.size();
}
//---------------------------------------------------------------------------
class CSentinelHacker
{
  protected:
    vector<CTransmitter*>         CSentinelTransmitters;
    vector<CReceiver*>            CSentinelReceivers;
    unsigned                      CThrCount;
    vector<thread>                CThreadsWo;
    vector<thread>                CThreadsTr;
    vector<thread>                CThreadsRe;
    map< uint32_t, CSentinelMessage*>  CSentinelStorage;
    mutex                         CMutex;
    CSentinelBuffer               CBufferSolve;
    CSentinelBuffer               CBufferSend;

  public:
                             ~CSentinelHacker              ();
                             CSentinelHacker               ();
    static bool              SeqSolve                      ( const vector<uint64_t> & fragments,
                                                             CBigInt         & res );
    void                     AddTransmitter                ( ATransmitter      x );
    void                     AddReceiver                   ( AReceiver         x );
    void                     AddFragment                   ( uint64_t          x );
    void                     Start                         ( unsigned          thrCount );
    void                     Stop                          ( void );

    void                     CSentinelListener             ( int );
    void                     CSentinelTransmitter          ( int );
    void                     CSentinelSolver               ( int );
};
CSentinelHacker::CSentinelHacker(){
}

CSentinelHacker::~CSentinelHacker(){
  std::map<uint32_t, CSentinelMessage *>::iterator it = CSentinelStorage.begin();
  while(it!= CSentinelStorage.end()){
    delete CSentinelStorage[it->first];
    it++;
  }
}
// TODO: CSentinelHacker implementation goes here
bool CSentinelHacker::SeqSolve( const vector<uint64_t> & fragments, CBigInt & res ){
  auto outputSolver = [&res](const uint8_t * data, size_t bits) {
    CBigInt tmpExpr = CountExpressions(data+4, bits-32);
    if (res.CompareTo(tmpExpr) == -1){
      res = tmpExpr;
    }
  };
  return !!FindPermutations( &fragments.front(), fragments.size(), outputSolver);
}

void CSentinelHacker::AddTransmitter( ATransmitter x ){
  CSentinelTransmitters.push_back(x.get());
}

void CSentinelHacker::AddReceiver( AReceiver x ){
  CSentinelReceivers.push_back(x.get());
}

void CSentinelHacker::AddFragment( uint64_t x ){
  uint32_t keyC = CSentinelMessage::getFragmentId(x);
  CSharedBox * newFrg = NULL;
  //std::this_thread::sleep_for(std::chrono::milliseconds(700));
  newFrg = new CSharedBox(keyC, false);
  newFrg->setFragments(x);
  CBufferSolve.push_back(newFrg);
}

void CSentinelHacker::CSentinelListener( int lid ){
  uint64_t dataC;
  uint32_t keyC;
  CSharedBox * newFrg = NULL;
  while ( CSentinelReceivers[lid]->Recv(dataC) != false ){ 
    //std::this_thread::sleep_for(std::chrono::milliseconds(700));
    keyC = CSentinelMessage::getFragmentId(dataC);
    newFrg = new CSharedBox(keyC, false);
    newFrg->setFragments(dataC);
    CBufferSolve.push_back(newFrg);
  }
}

void CSentinelHacker::CSentinelTransmitter( int tid ){
  while(true){
    //std::this_thread::s_for(std::chrono::milliseconds(700));
    CSharedBox * box = CBufferSend.pop_front();
    if (box->die){
      delete box;
      break;
    }
    CSentinelTransmitters[tid]->Send(box->origin, box->res);
    delete box;
  }
}

void CSentinelHacker::CSentinelSolver( int sid ){
  uint32_t tmpOrig;
  CSharedBox * newSend;
  CSharedBox * box;
  vector<uint64_t> tmpVec;
  
  while(true){  
    box = CBufferSolve.pop_front();    
    
    if (box->die){
      delete box;
      break;
    }

    unique_lock<mutex> ul(CMutex);
    if (CSentinelStorage.find(box->origin)==CSentinelStorage.end()){
      CSentinelStorage[box->origin] = new CSentinelMessage(box->origin);
    }
    CSentinelStorage[box->origin]->CFragments.push_back(box->fragments);
    tmpVec = CSentinelStorage[box->origin]->CFragments;
    ul.unlock();

    tmpOrig = box->origin;
    CBigInt res;
  
    if ( SeqSolve ( tmpVec, res) ){
      newSend = new CSharedBox(box->origin, false);
      newSend->setRes(res);
      CBufferSend.push_back(newSend);
      ul.lock();
      delete CSentinelStorage[tmpOrig];
      CSentinelStorage.erase(tmpOrig);
      ul.unlock();
    }
    delete box;
  }
}

void CSentinelHacker::Start( unsigned thrCount ){
  CThrCount = thrCount;
  //Creating receivers
  for (size_t i = 0; i < CSentinelReceivers.size(); i++)
    CThreadsRe.push_back(thread(&CSentinelHacker::CSentinelListener, this, i));
  //creating transmitters
  for (size_t i = 0; i < CSentinelTransmitters.size(); i++)
    CThreadsTr.push_back(thread(&CSentinelHacker::CSentinelTransmitter, this, i));
  //Creating workers
  for (size_t i = 0; i < CThrCount; i++){
    CThreadsWo.push_back(thread(&CSentinelHacker::CSentinelSolver, this, i));
  }
}

void CSentinelHacker::Stop( void ){
  CSharedBox * newDeath = NULL;

  for (auto & i : CThreadsRe)
    i.join();    

  for(size_t i = 0; i < CThrCount ; i++){
    newDeath = new CSharedBox(0, true);
    CBufferSolve.push_back(newDeath);
  }  

  for (auto & i : CThreadsWo)
   i.join();

  for(size_t i = 0; i < CSentinelTransmitters.size(); i++){
    newDeath = new CSharedBox(0, true);
    CBufferSend.push_back(newDeath);
  }

  for (auto & i : CThreadsTr)
    i.join();
  
  //std::this_thread::sleep_for(std::chrono::milliseconds(700));
  std::map<uint32_t, CSentinelMessage *>::iterator it = CSentinelStorage.begin();
  while(it!= CSentinelStorage.end()){
    CSentinelTransmitters[0]->Incomplete(it->first);
    delete CSentinelStorage[it->first];
    it++;
  }
  CSentinelStorage.clear();
}
//-------------------------------------------------------------------------------------------------
#ifndef __PROGTEST__
int                main                                    ( void )
{
  
  using namespace std::placeholders;
  for ( const auto & x : g_TestSets )
  {
    CBigInt res;
    assert ( CSentinelHacker::SeqSolve ( x . m_Fragments, res ) );
    assert ( CBigInt ( x . m_Result ) . CompareTo ( res ) == 0 );
  }
  CSentinelHacker test;
  auto            trans = make_shared<CExampleTransmitter> ();
  AReceiver       recv  = make_shared<CExampleReceiver> ( initializer_list<uint64_t> { 0x02230000000c, 0x071e124dabef, 0x02360037680e, 0x071d2f8fe0a1, 0x055500150755 } );
  
  test . AddTransmitter ( trans ); 
  test . AddReceiver ( recv ); 
  
  test . Start ( 3 );
  
  static initializer_list<uint64_t> t1Data = { 0x071f6b8342ab, 0x0738011f538d, 0x0732000129c3, 0x055e6ecfa0f9, 0x02ffaa027451, 0x02280000010b, 0x02fb0b88bc3e };
  thread t1 ( FragmentSender, bind ( &CSentinelHacker::AddFragment, &test, _1 ), t1Data );
  static initializer_list<uint64_t> t2Data = { 0x073700609bbd, 0x055901d61e7b, 0x022a0000032b, 0x016f0000edfb };
  thread t2 ( FragmentSender, bind ( &CSentinelHacker::AddFragment, &test, _1 ), t2Data );
  FragmentSender ( bind ( &CSentinelHacker::AddFragment, &test, _1 ), initializer_list<uint64_t> { 0x017f4cb42a68, 0x02260000000d, 0x072500000025 } );
  t1 . join ();
  t2 . join ();
  test . Stop ();

  assert ( trans -> TotalSent () == 4 );
  assert ( trans -> TotalIncomplete () == 2 );  
  
  return 0;  
}
#endif /* __PROGTEST__ */ 
