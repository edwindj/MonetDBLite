import os, sys


def main():
    dir = os.getenv('TSTSRCDIR')
    clcmd = str(os.getenv('SQL_CLIENT'))
    clcmd1 = str(os.getenv('SQL_CLIENT')) + " -uskyserver -Pskyserver"
    sys.stdout.write('Create User\n')
    clt = os.popen(clcmd + "<%s" % ('%s/../create_user.sql' % dir), 'w')
    clt.close()
    sys.stdout.write('tables\n')
    clt1 = os.popen(clcmd1 + "<%s" % ('%s/../../../sql/math.sql' % dir), 'w')
    clt1.close()
    clt1 = os.popen(clcmd1 + "<%s" % ('%s/../../../sql/cache.sql' % dir), 'w')
    clt1.close()
    clt1 = os.popen(clcmd1 + "<%s" % ('%s/../../../sql/skyserver.sql' % dir), 'w')
    clt1.close()
    clt1 = os.popen(clcmd1 + "<%s" % ('%s/../Skyserver_tables.sql' % dir), 'w')
    clt1.close()
    clt1 = os.popen("cat %s/../Skyserver_import.sql | sed -e \"s|DATA|%s/../microsky|g\" | %s " % (dir, dir, clcmd1), 'w')
    clt1.close()
    clt1 = os.popen(clcmd1 + "<%s" % ('%s/../Skyserver_constraints.sql' % dir), 'w')
    clt1.close()
    sys.stdout.write('views\n')
    clt1 = os.popen(clcmd1 + "<%s" % ('%s/../Skyserver_views.sql' % dir), 'w')
    clt1.close()
    sys.stdout.write('functions\n')
    clt1 = os.popen(clcmd1 + "<%s" % ('%s/../Skyserver_functions.sql' % dir), 'w')
    clt1.close()
    sys.stdout.write('Cleanup\n')
    clt1 = os.popen(clcmd1 + "<%s" % ('%s/../Skyserver_dropFunctions.sql' % dir), 'w')
    clt1.close()
    clt1 = os.popen(clcmd1 + "<%s" % ('%s/../Skyserver_dropMs_functions.sql' % dir), 'w')
    clt1.close()
    clt1 = os.popen(clcmd1 + "<%s" % ('%s/../Skyserver_dropMath.sql' % dir), 'w')
    clt1.close()
    clt1 = os.popen(clcmd1 + "<%s" % ('%s/../Skyserver_dropCache.sql' % dir), 'w')
    clt1.close()
    clt1 = os.popen(clcmd1 + "<%s" % ('%s/../Skyserver_dropViews.sql' % dir), 'w')
    clt1.close()
    clt1 = os.popen(clcmd1 + "<%s" % ('%s/../Skyserver_dropConstraints.sql' % dir), 'w')
    clt1.close()
    clt1 = os.popen(clcmd1 + "<%s" % ('%s/../Skyserver_dropTables.sql' % dir), 'w')
    clt1.close()
    sys.stdout.write('Remove User\n')
    clt = os.popen(clcmd + "<%s" % ('%s/../drop_user.sql' % dir), 'w')
    clt.close()

main()
