/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

const PropTypes = require('prop-types');
const React = require('react');

function SocialFooter(props) {
  const repoUrl = `https://github.com/${props.config.organizationName}/${
    props.config.projectName
  }`;
  return (
    <div className="footerSection">
      <h5>Social</h5>
      <div className="social">
        <a
          className="github-button" // part of the https://buttons.github.io/buttons.js script in siteConfig.js
          href={repoUrl}
          data-count-href={`${repoUrl}/stargazers`}
          data-show-count="true"
          data-count-aria-label="# stargazers on GitHub"
          aria-label="Star this project on GitHub">
          {props.config.projectName}
        </a>
      </div>
    </div>
  );
}

SocialFooter.propTypes = {
  config: PropTypes.object,
};

class Footer extends React.Component {
  render() {
    const docsPart = `${
      this.props.config.docsUrl ? `${this.props.config.docsUrl}/` : ''
    }`;
    return (
      <footer className="nav-footer" id="footer">
        <section className="sitemap">
          {this.props.config.footerIcon && (
            <a href={this.props.config.baseUrl} className="nav-home">
              <img
                src={`${this.props.config.baseUrl}${
                  this.props.config.footerIcon
                }`}
                alt={this.props.config.title}
                width="66"
                height="58"
              />
            </a>
          )}
          <div className="footerSection">
            <h5>Docs</h5>
            <a
              href={`
                ${this.props.config.baseUrl}${docsPart}${
                this.props.language
              }/installation`}>
              Getting Started
            </a>
            <a
              href={`
                ${this.props.config.baseUrl}${docsPart}${
                this.props.language
              }/configuring`}>
              Configuring
            </a>
            <a
              href={`
                ${this.props.config.baseUrl}${docsPart}${
                this.props.language
              }/usage`}>
              Using
            </a>
            <a
              href={`
                ${this.props.config.baseUrl}${docsPart}${
                this.props.language
              }/faq`}>
              FAQ
            </a>
          </div>
          <SocialFooter config={this.props.config} />
        </section>
        <a
          href="https://opensource.facebook.com/"
          target="_blank"
          rel="noreferrer noopener"
          className="fbOpenSource">
          <img
            src={`${this.props.config.baseUrl}img/oss_logo.png`}
            alt="Facebook Open Source"
            width="170"
            height="45"
          />
        </a>
        <section className="copyright">
          {this.props.config.copyright && (
            <span>{this.props.config.copyright}</span>
          )}
        </section>
      </footer>
    );
  }
}

module.exports = Footer;
