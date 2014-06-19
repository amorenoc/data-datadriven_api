/******************************************************************************
 * Copyright (c) 2014, AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#include <ostream>
#include <qcc/Mutex.h>

#include "consumer.h"

namespace test_system_multipeer {
class Consumer::EmitNumListener :
    public datadriven::SignalListener<MultiPeerProxy, MultiPeerProxy::EmitNum> {
  public:
    EmitNumListener(int consId,
                    sem_t* sema) :
        consId(consId), sema(sema)
    {
    }

    /**
     * Add expected (object ID, signal number) mapping.
     */
    void Expect(int id, int num)
    {
        mutex.Lock();
        idNumMap[id] = num;
        idOk[id] = false;
        mutex.Unlock();
    }

    /**
     * Validate that all signals for requests we fired have been received.
     */
    void Validate()
    {
        for (map<int, bool>::iterator it = idOk.begin(); it != idOk.end(); ++it) {
            assert(true == it->second);
        }
    }

    /**
     * Reset the expected and received signal containers.
     */
    void Reset()
    {
        idNumMap.clear();
        idOk.clear();
    }

    void OnSignal(const MultiPeerProxy::EmitNum& signal)
    {
        const std::shared_ptr<MultiPeerProxy> mpp = signal.GetEmitter();
        int32_t id = mpp->GetProperties().id;

        mutex.Lock();
        if (signal.num == idNumMap[id]) {
            // signal from the object for which we fired the request
            cout << "Consumer " << consId << " receives signal with num "
                 << signal.num << " from object with id " << id << endl;
            idOk[id] = true;
            sem_post(sema);
        }
        mutex.Unlock();
    }

  private:
    mutable qcc::Mutex mutex;
    int consId;
    sem_t* sema;
    map<int, int> idNumMap;
    map<int, bool> idOk;
};

Consumer::Consumer(int consId,
                   int numObj) :
    consId(consId), numObj(numObj), busConnection(),
    observer(busConnection, this), enl(NULL)
{
    sem_init(&sync, 0, 0);
    assert(ER_OK == busConnection.GetStatus());
    enl = new EmitNumListener(consId, &sync);
    observer.AddSignalListener(*enl);
};

Consumer::~Consumer()
{
    observer.RemoveSignalListener(*enl);
    delete enl;
    sem_destroy(&sync);
}

void Consumer::OnUpdate(const shared_ptr<MultiPeerProxy>& mpp)
{
    int obj_id = mpp->GetProperties().id;

    cout << "Consumer " << consId << " receives object with id " << obj_id << endl;
    assert(objects.end() == objects.find(obj_id));
    objects[obj_id] = mpp;
    sem_post(&sync);
}

void Consumer::Test(int numLoops)
{
    // wait for objects
    for (int i = 0; i < numObj; i++) {
        cout << "Consumer " << consId << " waits for " << (numObj - i)
             << " object(s)" << endl;
        sem_wait(&sync);
    }
    // loop discovered objects in ascending ID order
    cout << "Consumer " << consId << " checks all objects discovered" << endl;
    for (int i = 0; i < numObj; i++) {
        assert(objects.end() != objects.find(i));
    }
    for (int loop = 1; loop <= numLoops; loop++) {
        int cnt = 0;

        cout << "Consumer " << consId << " starting loop " << loop << "/" << numLoops << endl;
        // iterate objects known to observer
        enl->Reset();
        for (datadriven::Observer<MultiPeerProxy>::iterator it = observer.begin();
             it != observer.end(); ++it) {
            int32_t id = it->GetProperties().id;
            int num = (consId * (numObj * numLoops)) + ((loop - 1) * numObj) + cnt;

            assert(objects[id] == *it);
            // call method that will trigger signal
            cout << "Consumer " << consId << " loop " << loop << "/" << numLoops
                 << " calls method on object with id "
                 << id << " and number " << num << endl;
            enl->Expect(id, num);
            it->RequestEmitNum(num);
            cnt++;
        }
        assert(numObj == cnt);
        // wait for signals
        for (int i = numObj; i > 0; i--) {
            cout << "Consumer " << consId << " loop " << loop << "/" << numLoops
                 << " waits for " << i << " signal(s)" << endl;
            sem_wait(&sync);
        }
        // validate all signals received
        cout << "Consumer " << consId << " loop " << loop << "/" << numLoops
             << " checks all signals received" << endl;
        enl->Validate();
    }
    // cleaning up
    objects.clear();
    // done
    cout << "Consumer " << consId << " is done" << endl;
}
};
