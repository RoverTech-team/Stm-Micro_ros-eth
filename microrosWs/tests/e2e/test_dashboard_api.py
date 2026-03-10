import pytest
import requests
from typing import Dict, Any


pytestmark = pytest.mark.e2e


class TestHealthCheck:
    @pytest.mark.requires_docker
    def test_health_check_returns_healthy(self, dashboard_client):
        response = dashboard_client.get("/health")

        assert response.status_code == 200
        data = response.json()

        assert data["status"] == "healthy"
        assert "timestamp" in data
        assert "version" in data

    @pytest.mark.requires_docker
    def test_health_check_has_ros_status(self, dashboard_client):
        response = dashboard_client.get("/health")

        assert response.status_code == 200
        data = response.json()

        assert "ros_connected" in data
        assert isinstance(data["ros_connected"], bool)


class TestNodesEndpoint:
    @pytest.mark.requires_docker
    def test_nodes_endpoint_returns_list(self, dashboard_client):
        response = dashboard_client.get("/api/nodes")

        assert response.status_code == 200
        data = response.json()

        assert isinstance(data, list)

    @pytest.mark.requires_docker
    def test_nodes_have_required_fields(self, dashboard_client):
        response = dashboard_client.get("/api/nodes")

        assert response.status_code == 200
        nodes = response.json()

        if len(nodes) > 0:
            node = nodes[0]
            required_fields = ["id", "name", "status", "type", "health_score"]
            for field in required_fields:
                assert field in node, f"Missing required field: {field}"


class TestSystemStatus:
    @pytest.mark.requires_docker
    def test_system_status_returns_status(self, dashboard_client):
        response = dashboard_client.get("/api/system_status")

        assert response.status_code == 200
        data = response.json()

        assert "status" in data
        assert "nodes_online" in data
        assert "total_nodes" in data
        assert "timestamp" in data

    @pytest.mark.requires_docker
    def test_system_status_has_valid_status_value(self, dashboard_client):
        response = dashboard_client.get("/api/system_status")

        assert response.status_code == 200
        data = response.json()

        valid_statuses = ["active", "standby", "waiting_for_nodes", "error", "unknown"]
        assert data["status"] in valid_statuses

    @pytest.mark.requires_docker
    def test_system_status_nodes_online_is_integer(self, dashboard_client):
        response = dashboard_client.get("/api/system_status")

        assert response.status_code == 200
        data = response.json()

        assert isinstance(data["nodes_online"], int)
        assert isinstance(data["total_nodes"], int)


class TestNodeDetail:
    @pytest.mark.requires_docker
    def test_node_detail_returns_node(self, dashboard_client):
        nodes = dashboard_client.get_nodes()

        if len(nodes) == 0:
            pytest.skip("No nodes available for testing")

        node_id = nodes[0]["id"]
        response = dashboard_client.get(f"/api/nodes/{node_id}")

        assert response.status_code == 200
        data = response.json()

        assert data["id"] == node_id

    @pytest.mark.requires_docker
    def test_node_detail_nonexistent_returns_404(self, dashboard_client):
        response = dashboard_client.get("/api/nodes/99999")

        assert response.status_code == 404
        data = response.json()

        assert "error" in data


class TestUpdateNodeAuth:
    @pytest.mark.requires_docker
    def test_update_node_requires_auth(self, docker_compose_stack):
        base_url = docker_compose_stack["dashboard_url"]

        response = requests.post(
            f"{base_url}/api/update_node",
            json={"node_id": 1, "status": "standby"},
            headers={"Content-Type": "application/json"},
        )

        assert response.status_code == 401

    @pytest.mark.requires_docker
    def test_update_node_wrong_credentials(self, docker_compose_stack):
        base_url = docker_compose_stack["dashboard_url"]

        response = requests.post(
            f"{base_url}/api/update_node",
            json={"node_id": 1, "status": "standby"},
            headers={"Content-Type": "application/json"},
            auth=("wronguser", "wrongpass"),
        )

        assert response.status_code == 401


class TestUpdateNodeValid:
    @pytest.mark.requires_docker
    def test_update_node_with_valid_data(self, dashboard_client):
        nodes = dashboard_client.get_nodes()

        if len(nodes) == 0:
            pytest.skip("No nodes available for testing")

        node_id = nodes[0]["id"]

        result = dashboard_client.update_node(node_id, status="standby")

        assert result["success"] is True
        assert str(node_id) in result["message"]
        assert "status" in result["updated_fields"]

    @pytest.mark.requires_docker
    def test_update_node_health_score(self, dashboard_client):
        nodes = dashboard_client.get_nodes()

        if len(nodes) == 0:
            pytest.skip("No nodes available for testing")

        node_id = nodes[0]["id"]

        result = dashboard_client.update_node(node_id, health_score=75)

        assert result["success"] is True
        assert "health_score" in result["updated_fields"]
        assert result["node"]["health_score"] == 75

    @pytest.mark.requires_docker
    def test_update_node_returns_updated_node(self, dashboard_client):
        nodes = dashboard_client.get_nodes()

        if len(nodes) == 0:
            pytest.skip("No nodes available for testing")

        node_id = nodes[0]["id"]

        result = dashboard_client.update_node(node_id, status="active", health_score=88)

        assert "node" in result
        assert result["node"]["id"] == node_id


class TestFailuresEndpoint:
    @pytest.mark.requires_docker
    def test_failures_endpoint_returns_list(self, dashboard_client):
        response = dashboard_client.get("/api/failures")

        assert response.status_code == 200
        data = response.json()

        assert isinstance(data, list)

    @pytest.mark.requires_docker
    def test_failures_have_required_fields(self, dashboard_client):
        response = dashboard_client.get("/api/failures")

        assert response.status_code == 200
        failures = response.json()

        if len(failures) > 0:
            failure = failures[0]
            required_fields = ["id", "timestamp", "node_id", "description"]
            for field in required_fields:
                assert field in failure, f"Missing required field: {field}"
