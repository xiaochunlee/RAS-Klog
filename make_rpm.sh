#! /bin/sh
#
# make klog-{version}.rpm  klog-{version}.src.rpm
#


tgz_name=""

function get_tgz_name()
{
        name=`sed '/^Name:/!d;s/.*: //' klog.spec`
        version=`sed '/^Version:/!d;s/.*: //' klog.spec`

        tgz_name=${name}-${version}
}


get_tgz_name

ln -sf ${PWD} /tmp/${tgz_name}
cd /tmp/
tar -zchf ${tgz_name}.tgz ${tgz_name}
rpmbuild -ta ${tgz_name}.tgz

rm -rf ${tgz_name}
rm -rf ${tgz_name}.tgz
