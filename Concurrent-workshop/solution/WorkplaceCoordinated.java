package cp2022.solution;

import cp2022.base.Workplace;

import java.util.*;
import java.util.concurrent.Semaphore;

import static cp2022.solution.Utils.*;

public class WorkplaceCoordinated extends Workplace {
    private final Workplace uncoordinatedWorkplace;
    private final WorkshopCoordinated ourWorkshop;
    private final HashSet<Long> processesWaitingToEnter = new HashSet<>();
    private final Set<Long> processesWaitingToSwitch = new HashSet<>();
    private final HashMap<Long, WorkplaceCoordinated> pastProcessWorkplace;
    private final Semaphore mutex;
    private final Semaphore workplaceSemaphore = new Semaphore(0);
    private int numberOfProcessesOnWorkplace = 0;
    private boolean someoneWantsToWork = false;

    protected WorkplaceCoordinated(Workplace uncoordinatedWorkplace,
                                   WorkshopCoordinated ourWorkshop, Semaphore mutex,
                                   HashMap<Long, WorkplaceCoordinated> pastProcessWorkplace
    ) {
        super(uncoordinatedWorkplace.getId());
        this.uncoordinatedWorkplace = uncoordinatedWorkplace;
        this.ourWorkshop = ourWorkshop;
        this.mutex = mutex;
        this.pastProcessWorkplace = pastProcessWorkplace;
    }

    public int numberOfProcessesOnWorkplace() {return this.numberOfProcessesOnWorkplace;}
    public void processComesToWorkplace() {
        this.numberOfProcessesOnWorkplace += 1;
    }
    public void processLeavesWorkplace() {this.numberOfProcessesOnWorkplace -= 1;}
    public void letSomeoneWork() {releaseSemaphore(workplaceSemaphore);}
    public boolean isSomeoneWaitingToSwitch() {return processesWaitingToSwitch.size() > 0;}
    public void letProcessSwitch(Long tid) {processesWaitingToSwitch.remove(tid);}
    public void letProcessEnter(Long tid) {processesWaitingToEnter.remove(tid);}
    public Long getSomeoneToSwitch() {return processesWaitingToSwitch.iterator().next();}
    public Long getSomeoneToEnter() {return processesWaitingToEnter.iterator().next();}
    public void someoneNewWantsToSwitch(Long tid) {processesWaitingToSwitch.add(tid);}
    public void someoneNewWantsToEnter(Long tid) {processesWaitingToEnter.add(tid);}
    public boolean isSomeoneWaitingToEnter() {return processesWaitingToEnter.size() > 0;}
    public boolean someoneWantsToWork() {return this.someoneWantsToWork;}
    public void weAllowedSomeoneToWork() {this.someoneWantsToWork = false;}


    @Override
    public void use() {
        acquireSemaphore(mutex);
        WorkplaceCoordinated lastWorkplace = pastProcessWorkplace.get(Thread.currentThread().getId());
        boolean didSomeoneEnter = false;
        if (lastWorkplace != null) {
            lastWorkplace.processLeavesWorkplace();
            if (lastWorkplace.numberOfProcessesOnWorkplace() == 1 && lastWorkplace.someoneWantsToWork()) {
                lastWorkplace.weAllowedSomeoneToWork();
                lastWorkplace.letSomeoneWork();
            }
            else if(ourWorkshop.isSomeoneWaitingToSwitch(lastWorkplace)){
                didSomeoneEnter = true;
                ourWorkshop.letSomeoneSwitch(lastWorkplace);
            }
            else if (!ourWorkshop.isSomeoneWaitingTooLong() && ourWorkshop.canSomeoneEnter()){
                didSomeoneEnter = true;
                ourWorkshop.letSomeoneEnter();
            }
            else if (ourWorkshop.isSomeoneWaitingTooLong() && ourWorkshop.canLastEnter()){
                didSomeoneEnter = true;
                ourWorkshop.letLongestWaitingProcessEnter();
            }
        }   

        if (numberOfProcessesOnWorkplace == 2) {
            someoneWantsToWork = true;
            if (!didSomeoneEnter)
                releaseSemaphore(mutex);
            acquireSemaphore(workplaceSemaphore);
        } else if (!didSomeoneEnter){
            releaseSemaphore(mutex);
        }

        uncoordinatedWorkplace.use();

        acquireSemaphore(mutex);
        pastProcessWorkplace.put(Thread.currentThread().getId(), this);
        releaseSemaphore(mutex);
    }
}
