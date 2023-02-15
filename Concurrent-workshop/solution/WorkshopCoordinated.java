package cp2022.solution;

import java.util.concurrent.Semaphore;
import java.util.*;

import cp2022.base.*;
import static cp2022.solution.Utils.*;

public class WorkshopCoordinated implements Workshop {
    private final HashMap<Long, Semaphore> entrySemaphores = new HashMap<>();
    private final HashMap<Long, WorkplaceId> entryInformation = new HashMap<>();
    private final HashMap<Long, Integer> waitingTimeOnEntry = new HashMap<>();
    private final HashMap<Long, Semaphore> switchSemaphores = new HashMap<>();
    private final HashMap<Long, WorkplaceId> switchInformation = new HashMap<>();
    private final LinkedList<EntryInformation> waitingQueue = new LinkedList<>();
    private Integer maxWaitingTime;
    private final HashMap<WorkplaceId, WorkplaceCoordinated> workplaces = new HashMap<>();
    private final HashMap<WorkplaceCoordinated, Long> wantingToSwitchFromWorkplace = new HashMap<>();
    private final HashMap<Long, WorkplaceCoordinated> pastProcessWorkplace = new HashMap<>();
    private final Semaphore mutex = new Semaphore(1);
    private LinkedList<Long> potentialCycle = null;

    public WorkshopCoordinated(Collection<Workplace> workplaces) {
        for (Workplace w : workplaces) {
            WorkplaceId wid = w.getId();
            this.workplaces.put(wid, new WorkplaceCoordinated(w, this, mutex, pastProcessWorkplace));
            maxWaitingTime = 2 * workplaces.size() - 1;
        }
    }

    private boolean isSomeoneOnWorkstation(WorkplaceId wid){return workplaces.get(wid).numberOfProcessesOnWorkplace() > 0;}
    private boolean thereIsCycle(WorkplaceId nextWorkplace, WorkplaceId currentWorkplace){
        Long processOnNextWorkplace;
        LinkedList<Long> wakingUp = new LinkedList<>();
        while((processOnNextWorkplace = wantingToSwitchFromWorkplace.get(workplaces.get(nextWorkplace))) != null){
            WorkplaceId nextSwitch = switchInformation.get(processOnNextWorkplace);
            wakingUp.add(processOnNextWorkplace);
            if (nextSwitch == currentWorkplace){
                potentialCycle = wakingUp;
                return true;
            }
            nextWorkplace = nextSwitch;
        }
        return false;
    }

    private void updateEntryInformation(Long tid) {
        int waitingTime = waitingTimeOnEntry.get(tid);

        for (Map.Entry<Long, Integer> entry : waitingTimeOnEntry.entrySet()) {
            if (entry.getValue() >= waitingTime)
                entry.setValue(entry.getValue() + 1);
        }

        Iterator<EntryInformation> queueIterator = waitingQueue.iterator();
        while(queueIterator.hasNext()){
            EntryInformation next = queueIterator.next();
            if (next.waitingTime >= waitingTime){
                next.updateWaitingTime();
            }
            if(next.tid.equals(tid)){
                waitingTimeOnEntry.remove(next.tid);
                entryInformation.remove(tid);
                entrySemaphores.remove(tid);
                queueIterator.remove();
            }
        }
    }

    private void updateWaitingTime() {
        // we update all information
        this.waitingTimeOnEntry.replaceAll((id, time) -> time + 1);
        for (EntryInformation information : waitingQueue) {
            information.updateWaitingTime();
        }

    }

    public boolean isSomeoneWaitingTooLong() {
        return !waitingQueue.isEmpty() && waitingQueue.getFirst().waitingTime >= maxWaitingTime;
    }

    private Long longestWaitingProcess() {
        return waitingQueue.getFirst().tid;
    }

    public void letSomeoneEnter() {
        for (WorkplaceCoordinated w : workplaces.values()){
            if (w.numberOfProcessesOnWorkplace() == 0 && w.isSomeoneWaitingToEnter()) {
                Long processToEnter = w.getSomeoneToEnter();
                releaseSemaphore(entrySemaphores.get(processToEnter));
                return;
            }
        }
    }

    public void letLongestWaitingProcessEnter(){
        Long tid = longestWaitingProcess();
        releaseSemaphore(entrySemaphores.get(tid));
    }

    public boolean canSomeoneEnter() {
        for (WorkplaceCoordinated w : workplaces.values()){
            if(w.numberOfProcessesOnWorkplace() == 0 && w.isSomeoneWaitingToEnter()){
                return true;
            }
        }
        return false;
    }

    public boolean canLastEnter() {
        if (waitingQueue.isEmpty()) return false;
        Long tid = longestWaitingProcess();
        WorkplaceCoordinated w = workplaces.get(entryInformation.get(tid));
        return w.numberOfProcessesOnWorkplace() == 0;
    }

    public boolean isSomeoneWaitingToSwitch(WorkplaceCoordinated lastWorkplace){
        return lastWorkplace.numberOfProcessesOnWorkplace() == 0 && lastWorkplace.isSomeoneWaitingToSwitch();
    }

    public void letSomeoneSwitch(WorkplaceCoordinated lastWorkplace) {
        Long wakingUpProcess = lastWorkplace.getSomeoneToSwitch();
        releaseSemaphore(switchSemaphores.get(wakingUpProcess));
    }

    @Override
    public Workplace enter(WorkplaceId wid) {
        acquireSemaphore(mutex);
        WorkplaceCoordinated currentWorkplace = workplaces.get(wid);
        Long tid = Thread.currentThread().getId();

        if (isSomeoneOnWorkstation(wid) || isSomeoneWaitingTooLong()) {
            currentWorkplace.someoneNewWantsToEnter(tid);
            waitingQueue.add(new EntryInformation(tid, wid, 0));
            entrySemaphores.put(tid, new Semaphore(0));
            waitingTimeOnEntry.put(tid, 0);
            entryInformation.put(tid, wid);
            releaseSemaphore(mutex);
            acquireSemaphore(this.entrySemaphores.get(tid));

            // we're woken up here
            currentWorkplace.letProcessEnter(tid);
            updateEntryInformation(tid);

        }
        else{
            // if we just eneter workspace without blocking our way
            updateWaitingTime();
        }
        workplaces.get(wid).processComesToWorkplace();
        releaseSemaphore(mutex);

        return workplaces.get(wid);
    }

    @Override
    public Workplace switchTo(WorkplaceId wid) {
        acquireSemaphore(mutex);
        Long tid = Thread.currentThread().getId();
        WorkplaceCoordinated lastWorkplace = pastProcessWorkplace.get(tid);
        WorkplaceCoordinated currentWorkplace = workplaces.get(wid);
        if (lastWorkplace.getId() != wid) {

            if (thereIsCycle(wid, lastWorkplace.getId())) {
                Long wakingUp = potentialCycle.pollLast();
                currentWorkplace.processComesToWorkplace();
                releaseSemaphore(switchSemaphores.get(wakingUp));
                return workplaces.get(wid);
            }

            if (isSomeoneOnWorkstation(wid)) {
                // we will be hung up on switch
                currentWorkplace.someoneNewWantsToSwitch(tid);

                // information for switches
                wantingToSwitchFromWorkplace.put(lastWorkplace, Thread.currentThread().getId());
                switchInformation.put(Thread.currentThread().getId(), wid);
                switchSemaphores.put(Thread.currentThread().getId(), new Semaphore(0));

                // we wait on our private semaphore
                releaseSemaphore(mutex);
                acquireSemaphore(switchSemaphores.get(Thread.currentThread().getId()));

                // we're woken up, need to remove information about switches
                currentWorkplace.letProcessSwitch(tid);
                switchSemaphores.remove(Thread.currentThread().getId());
                switchInformation.remove(Thread.currentThread().getId());
                wantingToSwitchFromWorkplace.remove(lastWorkplace, Thread.currentThread().getId());

                // we wake up as part of cycle
                if (potentialCycle != null){
                    currentWorkplace.processComesToWorkplace();
                    if (!potentialCycle.isEmpty()) {
                        Long wakingUp = potentialCycle.pollLast();
                        releaseSemaphore(switchSemaphores.get(wakingUp));
                    }
                    else{
                        potentialCycle = null;
                        releaseSemaphore(mutex);
                    }
                    return workplaces.get(wid);
                }
            }

            currentWorkplace.processComesToWorkplace();
            // if we're not part of cycle, we can switch
            if (lastWorkplace.numberOfProcessesOnWorkplace() == 1 && lastWorkplace.isSomeoneWaitingToSwitch()){
                letSomeoneSwitch(lastWorkplace);
            }
            else{
                releaseSemaphore(mutex);
            }
        }
        else{
            // if the process wants to switch to self station
            currentWorkplace.processComesToWorkplace();
            releaseSemaphore(mutex);
        }

        return workplaces.get(wid);
    }

    @Override
    public void leave() {
        acquireSemaphore(mutex);
        WorkplaceCoordinated leavingWorkplace = pastProcessWorkplace.get(Thread.currentThread().getId());
        pastProcessWorkplace.put(Thread.currentThread().getId(), null);

        leavingWorkplace.processLeavesWorkplace();
        // if we leave, but we were in the middle of switching places
        if (leavingWorkplace.numberOfProcessesOnWorkplace() == 1 && leavingWorkplace.someoneWantsToWork()) {
            leavingWorkplace.weAllowedSomeoneToWork();
            leavingWorkplace.letSomeoneWork();
        }
        // if we leave but someone is wanting to switch places, we have to allow him
         else if (isSomeoneWaitingToSwitch(leavingWorkplace)) {
            letSomeoneSwitch(leavingWorkplace);
        } else if (!isSomeoneWaitingTooLong() && canSomeoneEnter()) {
            letSomeoneEnter();
        }
         // if we have to let the last process enter
         else if (isSomeoneWaitingTooLong() && canLastEnter()) {
             letLongestWaitingProcessEnter();
        }
         else{
            releaseSemaphore(mutex);
        }

    }
}
