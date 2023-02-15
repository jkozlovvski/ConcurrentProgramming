package cp2022.solution;

import java.util.concurrent.Semaphore;

public final class Utils {
    public static void acquireSemaphore(Semaphore semaphore) {
        try {
            semaphore.acquire();
        } catch (InterruptedException e) {
            throw new RuntimeException("panic: unexpected thread interruption");
        }
    }

    public static void releaseSemaphore(Semaphore semaphore) {
        semaphore.release();
    }

}

