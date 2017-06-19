scriptDir=$(dirname `readlink -f "$0"`)

#Checking python packages
echo "Checking python packages ..."
while read package
do
  python -c "import $package" 2> /dev/null
  if [ $? -ne 0 ]
  then
    missing="$missing $package"
  fi
done < $scriptDir/requirements.txt

if [ ! -z "$missing" ]
then
  echo "ERROR: The following python packages are missing:"
  for package in $missing
  do
    echo "*   $package"
  done
  #echo "Please use easy_install or pip to install them and then try agin."
  echo "Installing now ..."
  set -e
  easy_install $package
  set +e
fi

echo "Checking building tools ..."
command -v gcc 2> /dev/null
if [ $? -ne 0 ]
then
  #echo "ERROR: gcc is missing. Please install build-essential"
  #exit 1
  set -e
  apt-get install build-essential
  set +e
fi

command -v mpirun mpicc 2> /dev/null
if [ $? -ne 0 ]
then
  #echo "ERROR: OpenMPI is missing. Please install openmpi-bin and libopenmpi-dev."
  #exit 1
  set -e
  apt-get install openmpi-bin
  apt-get install libopenmpi-dev
  set +e
fi


echo "Building mylib ..."
cd $scriptDir/mylib
make

cd $scriptDir
make

