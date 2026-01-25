import pytest


def pytest_addoption(parser):
    parser.addoption("--rtsp_server", action="store")
    parser.addoption("--motion_detection", action="store")
    parser.addoption("--resource_file", action="store", default="tests/res/test.264")


@pytest.fixture
def rtsp_server(request):
    return request.config.option.rtsp_server


@pytest.fixture
def motion_detection(request):
    return request.config.option.motion_detection


@pytest.fixture
def resource_file(request):
    return request.config.option.resource_file
