import pytest
from pathlib import Path


def pytest_addoption(parser):
    parser.addoption("--rtsp_server", action="store")
    parser.addoption("--motion_detection", action="store")
    parser.addoption("--resource_file", action="store", default="tests/res/test.264")


@pytest.fixture
def rtsp_server(request):
    return Path(request.config.option.rtsp_server).absolute()


@pytest.fixture
def motion_detection(request):
    return Path(request.config.option.motion_detection).absolute()


@pytest.fixture
def resource_file(request):
    return Path(request.config.option.resource_file).absolute()
