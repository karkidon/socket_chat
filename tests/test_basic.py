import os
import random
import pytest
import pexpect
import logging
import subprocess

logger = logging.getLogger(__name__)

EXEC_DIR = 'cmake-build-release'

PORT = random.randint(40000, 50000)


@pytest.fixture(scope="session", autouse=True)
def build_init(request):
    logger.info('COMPILING')
    cmake_result = subprocess.run(['./make-release.sh', ''])
    assert cmake_result.returncode == 0
    logger.info('COMPILATION SUCCESS')
    os.chdir(EXEC_DIR)
    logger.info('KEY GENERATION')
    gen_key_result = subprocess.run(['./server', '--gen-key'])
    assert gen_key_result.returncode == 0
    logger.info('KEY GENERATION SUCCESS')


@pytest.fixture(scope="session", autouse=True)
def server():
    logger.info('SPAWNING SERVER')
    instance = pexpect.spawn(f'./server -p={PORT}')
    try:
        instance.expect('.*Listener bound to.*')
    except pexpect.EOF:
        logger.critical('SERVER NOT SPAWNED')
        exit(1)
    logger.info('SPAWNING SERVER SUCCESS')
    yield instance


def spawn_client(number: int) -> pexpect.spawn:
    logger.info(f'SPAWNING CLIENT {number}')
    instance = pexpect.spawn(f'./client -p={PORT}', timeout=1)
    instance.logfile = open(f'client_{number}.txt', 'wb')
    instance.expect('.*exit')
    logger.info(f'SPAWNING CLIENT {number} SUCCESS')
    return instance


@pytest.fixture
def client_1():
    yield spawn_client(1)


@pytest.fixture
def client_2():
    yield spawn_client(2)


@pytest.fixture
def client_3():
    yield spawn_client(3)


def test_hello(client_1: pexpect.spawn, client_2: pexpect.spawn, client_3: pexpect.spawn):
    logger.info(f'STARTING TEST 1')
    client_1.sendline('Test message from client 1')
    try:
        client_2.expect('Test message from client 1')
        client_3.expect('Test message from client 1')
    except pexpect.exceptions.TIMEOUT:
        pytest.fail('EXPECT TIMED OUT!')
    logger.info(f'TEST 1 PASSED')

    logger.info(f'STARTING TEST 2')
    client_2.sendline('Test message from client 2')
    try:
        client_1.expect('Test message from client 2')
        client_3.expect('Test message from client 2')
    except pexpect.exceptions.TIMEOUT:
        pytest.fail('EXPECT TIMED OUT!')
    logger.info(f'TEST 2 PASSED')

    logger.info(f'STARTING TEST 3')
    client_3.sendline('Test message from client 3')
    try:
        client_1.expect('Test message from client 3')
        client_2.expect('Test message from client 3')
    except pexpect.exceptions.TIMEOUT:
        pytest.fail('EXPECT TIMED OUT!')
    logger.info(f'TEST 3 PASSED')
