package cp2022.solution;

import cp2022.base.WorkplaceId;

public class EntryInformation {
    Long tid;
    WorkplaceId wid;
    Integer waitingTime;

    public EntryInformation(Long tid, WorkplaceId wid, Integer waitingTime){
        this.tid = tid;
        this.wid = wid;
        this.waitingTime = waitingTime;
    }

    public void updateWaitingTime() {
        waitingTime += 1;
    }
}
