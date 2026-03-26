import json
import pytest
from app import app, get_default_data
from base64 import b64encode


@pytest.fixture
def client(tmp_path):
    """Create test client"""
    app.config['TESTING'] = True
    app.config['RATELIMIT_ENABLED'] = False  # Disable rate limiting for tests
    app.config['DATA_FILE'] = str(tmp_path / 'system_data.json')
    app.config['LOG_FILE'] = str(tmp_path / 'app.log')

    import app as microk3_app
    original_ros_available = microk3_app.ROS_AVAILABLE
    microk3_app.app.config['DATA_FILE'] = app.config['DATA_FILE']
    microk3_app.app.config['LOG_FILE'] = app.config['LOG_FILE']
    microk3_app.system_data = get_default_data()
    microk3_app.ros_inspector_state = microk3_app.get_empty_ros_state()
    microk3_app.ros_manager = None

    with app.test_client() as client:
        yield client

    microk3_app.ROS_AVAILABLE = original_ros_available


@pytest.fixture
def auth_headers():
    """Create authentication headers"""
    credentials = b64encode(b"admin:changeme").decode('utf-8')
    return {'Authorization': f'Basic {credentials}'}


def test_home_page(client):
    """Test home page loads"""
    response = client.get('/')
    assert response.status_code == 200


def test_nodes_page(client):
    """Test nodes page loads"""
    response = client.get('/nodes')
    assert response.status_code == 200


def test_ros_page(client):
    """Test ROS page loads"""
    response = client.get('/ros')
    assert response.status_code == 200


def test_health_check(client):
    """Test health endpoint"""
    response = client.get('/health')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert data['status'] == 'healthy'


def test_api_nodes(client):
    """Test nodes API endpoint"""
    response = client.get('/api/nodes')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert isinstance(data, list)
    assert len(data) == 0


def test_api_system_status(client):
    """Test system status endpoint"""
    response = client.get('/api/system_status')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert 'status' in data
    assert 'nodes_online' in data


def test_api_node_detail(client):
    """Test node detail endpoint"""
    response = client.get('/api/nodes/1')
    assert response.status_code == 404


def test_api_node_not_found(client):
    """Test node not found"""
    response = client.get('/api/nodes/999')
    assert response.status_code == 404


def test_update_node_no_auth(client):
    """Test update node without authentication"""
    import app as microk3_app
    microk3_app.system_data['nodes'].append(
        microk3_app.Node(
            id=1,
            name="Node 1",
            status="active",
            type="STM32",
            ram="128KB",
            flash="512KB",
            cpu="Cortex-M7"
        )
    )
    response = client.post('/api/update_node',
                          json={'node_id': 1, 'status': 'standby'},
                          content_type='application/json')
    assert response.status_code == 401


def test_update_node_with_auth(client, auth_headers):
    """Test update node with authentication"""
    import app as microk3_app
    microk3_app.system_data['nodes'].append(
        microk3_app.Node(
            id=1,
            name="Node 1",
            status="active",
            type="STM32",
            ram="128KB",
            flash="512KB",
            cpu="Cortex-M7"
        )
    )
    response = client.post('/api/update_node',
                          headers=auth_headers,
                          json={'node_id': 1, 'status': 'standby'},
                          content_type='application/json')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert data['success'] is True


def test_update_node_invalid_status(client, auth_headers):
    """Test update node with invalid status"""
    import app as microk3_app
    microk3_app.system_data['nodes'].append(
        microk3_app.Node(
            id=1,
            name="Node 1",
            status="active",
            type="STM32",
            ram="128KB",
            flash="512KB",
            cpu="Cortex-M7"
        )
    )
    response = client.post('/api/update_node',
                          headers=auth_headers,
                          json={'node_id': 1, 'status': 'invalid'},
                          content_type='application/json')
    assert response.status_code == 400


def test_update_node_missing_field(client, auth_headers):
    """Test update node with missing field"""
    import app as microk3_app
    microk3_app.system_data['nodes'].append(
        microk3_app.Node(
            id=1,
            name="Node 1",
            status="active",
            type="STM32",
            ram="128KB",
            flash="512KB",
            cpu="Cortex-M7"
        )
    )
    response = client.post('/api/update_node',
                          headers=auth_headers,
                          json={'status': 'active'},
                          content_type='application/json')
    assert response.status_code == 400


def test_add_failure_with_auth(client, auth_headers):
    """Test adding failure with authentication"""
    import app as microk3_app
    microk3_app.system_data['nodes'].append(
        microk3_app.Node(
            id=1,
            name="Node 1",
            status="active",
            type="STM32",
            ram="128KB",
            flash="512KB",
            cpu="Cortex-M7"
        )
    )

    response = client.post('/api/add_failure',
                          headers=auth_headers,
                          json={
                              'node_id': 1,
                              'description': 'Test failure',
                              'status': 'open'
                          },
                          content_type='application/json')
    assert response.status_code == 201
    data = json.loads(response.data)
    assert data['success'] is True


def test_api_ros_graph_disconnected(client):
    """ROS graph endpoint should return stable empty payload when ROS is disconnected."""
    response = client.get('/api/ros/graph')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert data['ros_connected'] is False
    assert data['nodes'] == []
    assert data['topics'] == []
    assert data['watched_topics'] == {}


def test_api_ros_watch_requires_topic_name(client):
    response = client.post('/api/ros/watch', json={}, content_type='application/json')
    assert response.status_code == 400


def test_api_ros_watch_disconnected(client):
    response = client.post(
        '/api/ros/watch',
        json={'topic_name': '/heartbeat'},
        content_type='application/json'
    )
    assert response.status_code == 503


def test_api_ros_unwatch_missing_topic(client):
    response = client.post('/api/ros/unwatch', json={}, content_type='application/json')
    assert response.status_code == 400


def test_api_ros_topic_samples_not_found(client):
    response = client.get('/api/ros/topics/heartbeat/samples')
    assert response.status_code == 404


def test_api_ros_watch_limit_enforced(client, monkeypatch):
    import app as microk3_app

    for idx in range(microk3_app.ROS_WATCH_LIMIT):
        topic_name = f'/topic_{idx}'
        microk3_app.ros_inspector_state['watched_topics'][topic_name] = {
            'topic_name': topic_name,
            'topic_type': 'std_msgs/msg/String',
            'latest': None,
            'history': [],
            'last_update': None,
            'status': 'idle',
            'error': None,
        }

    response = client.post(
        '/api/ros/watch',
        json={'topic_name': '/overflow'},
        content_type='application/json'
    )
    assert response.status_code == 400


def test_api_ros_watch_and_unwatch_with_fake_manager(client):
    import app as microk3_app

    class FakeRosManager:
        running = True

        def watch_topic(self, topic_name):
            microk3_app.ros_update_callback('watch_started', {
                'topic_name': topic_name,
                'topic_type': 'std_msgs/msg/String',
            })
            return {'success': True, 'topic_name': topic_name, 'topic_type': 'std_msgs/msg/String'}

        def unwatch_topic(self, topic_name):
            microk3_app.ros_update_callback('watch_stopped', {'topic_name': topic_name})
            return {'success': True, 'topic_name': topic_name}

    microk3_app.ROS_AVAILABLE = True
    microk3_app.ros_manager = FakeRosManager()

    watch_response = client.post(
        '/api/ros/watch',
        json={'topic_name': '/heartbeat'},
        content_type='application/json'
    )
    assert watch_response.status_code == 200

    graph_response = client.get('/api/ros/graph')
    graph_data = json.loads(graph_response.data)
    assert '/heartbeat' in graph_data['watched_topics']

    unwatch_response = client.post(
        '/api/ros/unwatch',
        json={'topic_name': '/heartbeat'},
        content_type='application/json'
    )
    assert unwatch_response.status_code == 200
