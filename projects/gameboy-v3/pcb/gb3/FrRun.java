import app.freerouting.core.RoutingJob;
import app.freerouting.interactive.HeadlessBoardManager;
import app.freerouting.management.RoutingJobScheduler;
import java.io.FileOutputStream;
import java.util.Locale;

/**
 * Runs FreeRouting 2.1.0's normal CLI, then writes the board it actually finished with.
 *
 * 2.1.0 writes -do from job.output, which is only re-serialised inside a board-updated event. After a
 * "Restoring an earlier board" step the router works on a copy that reaches job.board only when its
 * loop exits, with no event after that, so -do gets a stale board while the stats it prints (taken
 * from job.board) describe the real one. This hook serialises job.board itself, exactly the way the
 * scheduler's own setJobOutputToSpecctraSes does, just at the end.
 *
 * Usage: java -cp freerouting-2.1.0.jar:. FrRun <final.ses> <normal freerouting args...>
 */
public class FrRun {
    public static void main(String[] args) throws Exception {
        String out = args[0];
        String[] rest = java.util.Arrays.copyOfRange(args, 1, args.length);
        Runtime.getRuntime().addShutdownHook(new Thread(() -> {
            try {
                for (RoutingJob job : RoutingJobScheduler.getInstance().jobs) {
                    if (job.board == null) continue;
                    HeadlessBoardManager bm = new HeadlessBoardManager(Locale.US, job);
                    bm.replaceRoutingBoard(job.board);
                    try (FileOutputStream fo = new FileOutputStream(out)) {
                        boolean ok = bm.saveAsSpecctraSessionSes(fo, job.name);
                        System.err.println("FrRun: final board of job " + job.name + " -> " + out + " ok=" + ok);
                    }
                }
            } catch (Throwable t) {
                t.printStackTrace();
            }
        }));
        app.freerouting.Freerouting.main(rest);
    }
}
