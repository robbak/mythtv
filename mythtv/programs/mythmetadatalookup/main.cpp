// C headers
#include <unistd.h>

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QEventLoop>

// libmyth headers
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythversion.h"
#include "mythmiscutil.h"
#include "jobqueue.h"
#include "mythtranslation.h"
#include "mythconfig.h"
#include "commandlineparser.h"
#include "mythlogging.h"

#include "lookup.h"

namespace
{
    void cleanup()
    {
        delete gContext;
        gContext = NULL;

    }

    class CleanupGuard
    {
      public:
        typedef void (*CleanupFunc)();

      public:
        CleanupGuard(CleanupFunc cleanFunction) :
            m_cleanFunction(cleanFunction) {}

        ~CleanupGuard()
        {
            m_cleanFunction();
        }

      private:
        CleanupFunc m_cleanFunction;
    };
}

bool inJobQueue = false;

int main(int argc, char *argv[])
{
    MythMetadataLookupCommandLineParser cmdline;
    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toBool("showhelp"))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("showversion"))
    {
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHMETADATALOOKUP);

    int retval;
    if ((retval = cmdline.ConfigureLogging()) != GENERIC_EXIT_OK)
        return retval;

    ///////////////////////////////////////////////////////////////////////
    // Don't listen to console input
    close(0);

    CleanupGuard callCleanup(cleanup);

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    bool refreshall          = cmdline.toBool("refresh-all");
    bool refreshrules        = cmdline.toBool("refresh-rules");
    bool refreshallrules     = cmdline.toBool("refresh-all-rules");
    bool refreshallsafeart   = cmdline.toBool("refresh-all-artwork");
    bool refreshallart       = cmdline.toBool("refresh-all-artwork-dangerously");
    bool usedchanid          = cmdline.toBool("chanid");
    bool usedstarttime       = cmdline.toBool("starttime");
    bool addjob              = cmdline.toBool("jobid");

    int jobid            = cmdline.toInt("jobid");
    uint chanid          = cmdline.toUInt("chanid");
    QDateTime starttime  = cmdline.toDateTime("starttime");

    if (refreshallsafeart && (refreshall || refreshallrules ||
                              refreshallart || usedchanid || usedstarttime))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "--refresh-all-safe-art must not be accompanied by any other argument.");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (refreshallart && (refreshall || refreshallrules ||
                          refreshallsafeart || usedchanid || usedstarttime))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "--refresh-all-art must not be accompanied by any other argument.");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (refreshallrules && (refreshall || refreshallart ||
                            refreshallsafeart ||  usedchanid || usedstarttime))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "--refresh-all-rules must not be accompanied by any other argument.");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (refreshall && (refreshallrules || refreshallart ||
                       refreshallsafeart || usedchanid || usedstarttime))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "--refresh-all must not be accompanied by any other argument.");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (!addjob && !refreshall && !refreshallrules && !refreshallart &&
        !usedchanid && !usedstarttime && !refreshallsafeart)
    {
        refreshall = true;
    }

    if (addjob && (refreshall || refreshallrules || refreshallart ||
                   refreshallsafeart || usedchanid || usedstarttime))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "The jobqueue (-j) command cannot be used with other options.");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (!refreshall && !refreshallrules && !refreshallart && !addjob &&
        !refreshallsafeart && !(usedchanid && usedstarttime))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "--chanid and --starttime must be used together.");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    myth_nice(19);

    MythTranslation::load("mythfrontend");

    LookerUpper *lookup = new LookerUpper();

    if (addjob)
    {
        int type = JOB_METADATA;
        JobQueue::GetJobInfoFromID(jobid, type, chanid, starttime);
    }

    LOG(VB_GENERAL, LOG_INFO,
            "Testing grabbers and metadata sites for functionality...");
    if (!lookup->AllOK())
    {
        delete lookup;
        return GENERIC_EXIT_NOT_OK;
    }
    LOG(VB_GENERAL, LOG_INFO,
            "All grabbers tested and working.  Continuing...");

    if (refreshall)
    {
        lookup->CopyRuleInetrefsToRecordings();
        lookup->HandleAllRecordings(refreshrules);
    }
    else if (refreshallrules)
    {
        lookup->HandleAllRecordingRules();
        lookup->CopyRuleInetrefsToRecordings();
    }
    else if (refreshallsafeart)
    {
        lookup->HandleAllArtwork(false);
    }
    else if (refreshallart)
    {
        lookup->HandleAllArtwork(true);
    }
    else
        lookup->HandleSingleRecording(chanid, starttime, refreshrules);

    while (lookup->StillWorking())
    {
        sleep(1);
        qApp->processEvents();
    }

    delete lookup;

    LOG(VB_GENERAL, LOG_NOTICE, "MythMetadataLookup run complete.");

    return GENERIC_EXIT_OK;
}
