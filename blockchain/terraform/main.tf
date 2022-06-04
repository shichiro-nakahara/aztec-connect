terraform {
  backend "s3" {
    bucket = "aztec-terraform"
    key    = "aztec2/blockchain"
    region = "eu-west-2"
  }
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "3.74.2"
    }
  }
}

variable "ROLLUP_CONTRACT_ADDRESS" {
  type = string
}

output "rollup_contract_address" {
  value = "${var.ROLLUP_CONTRACT_ADDRESS}"
}

variable "PRICE_FEED_CONTRACT_ADDRESSES" {
  type = string
}

output "price_feed_contract_addresses" {
  value = "${var.PRICE_FEED_CONTRACT_ADDRESSES}"
}
